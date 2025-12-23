#include <iostream>
#include <vector>
#include <windows.h>
#include <string>
#include <random>

// Структура задачи
#pragma pack(push, 1)
struct Task {
    uint32_t type;          // 1 - задача, 255 - завершение
    uint32_t task_id;
    uint32_t data_size;     // количество пикселей
    // данные пикселей будут переданы после структуры
};

struct Result {
    uint32_t task_id;
    uint32_t result_size;
    // результаты будут переданы после структуры
};
#pragma pack(pop)

// Именованный мьютекс для синхронизации вывода между процессами
HANDLE console_mutex = NULL;

void print_safe(const std::string& message) {
    if (console_mutex) {
        WaitForSingleObject(console_mutex, INFINITE);
    }
    std::cout << message << std::endl;
    if (console_mutex) {
        ReleaseMutex(console_mutex);
    }
}

// Генерация случайных пикселей
std::vector<uint8_t> generate_pixels(size_t count) {
    std::vector<uint8_t> pixels(count);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < count; ++i) {
        pixels[i] = static_cast<uint8_t>(dis(gen));
    }
    return pixels;
}

class Browser {
private:
    struct WorkerInfo {
        HANDLE pipe_in = INVALID_HANDLE_VALUE;   // Канал для отправки задач в Worker
        HANDLE pipe_out = INVALID_HANDLE_VALUE;  // Канал для получения результатов
        PROCESS_INFORMATION process_info;
    };

    std::vector<WorkerInfo> workers;

public:
    ~Browser() {
        cleanup();
    }

    bool create_pipes_for_worker(int worker_id) {
        std::string pipe_in_name = "\\\\.\\pipe\\worker_in_" + std::to_string(worker_id);
        std::string pipe_out_name = "\\\\.\\pipe\\worker_out_" + std::to_string(worker_id);

        // Создаем канал для отправки задач В Worker (Browser пишет, Worker читает)
        HANDLE pipe_in = CreateNamedPipe(
            pipe_in_name.c_str(),
            PIPE_ACCESS_OUTBOUND,  // Browser пишет в этот канал
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,                      // максимальное количество экземпляров
            65536,                  // размер выходного буфера
            65536,                  // размер входного буфера
            0,                      // таймаут по умолчанию
            NULL
        );

        if (pipe_in == INVALID_HANDLE_VALUE) {
            print_safe("[Browser] Failed to create input pipe for worker " + std::to_string(worker_id));
            return false;
        }

        // Создаем канал для получения результатов ОТ Worker (Browser читает, Worker пишет)
        HANDLE pipe_out = CreateNamedPipe(
            pipe_out_name.c_str(),
            PIPE_ACCESS_INBOUND,    // Browser читает из этого канала
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            65536,
            65536,
            0,
            NULL
        );

        if (pipe_out == INVALID_HANDLE_VALUE) {
            print_safe("[Browser] Failed to create output pipe for worker " + std::to_string(worker_id));
            CloseHandle(pipe_in);
            return false;
        }

        WorkerInfo info;
        info.pipe_in = pipe_in;
        info.pipe_out = pipe_out;
        workers.push_back(info);

        print_safe("[Browser] Created pipes for worker " + std::to_string(worker_id));
        return true;
    }

    bool start_worker(int worker_id) {
        char exe_path[MAX_PATH];
        GetModuleFileName(NULL, exe_path, MAX_PATH);
        std::string exe_dir = exe_path;
        size_t pos = exe_dir.find_last_of("\\/");
        exe_dir = exe_dir.substr(0, pos);

        std::string worker_path = exe_dir + "\\Worker.exe";
        std::string command_line = "\"" + worker_path + "\" " + std::to_string(worker_id);

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        print_safe("[Browser] Starting worker " + std::to_string(worker_id));

        // Запускаем процесс Worker
        if (!CreateProcess(
            NULL,
            const_cast<LPSTR>(command_line.c_str()),
            NULL, NULL, FALSE,
            0,  // без новой консоли
            NULL, NULL,
            &si, &pi
        )) {
            print_safe("[Browser] Failed to start worker " + std::to_string(worker_id));
            return false;
        }

        workers[worker_id].process_info = pi;

        // Ждем подключения Worker к каналам
        if (!ConnectNamedPipe(workers[worker_id].pipe_in, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                print_safe("[Browser] Failed to connect to worker " + std::to_string(worker_id) + " input pipe");
                return false;
            }
        }

        if (!ConnectNamedPipe(workers[worker_id].pipe_out, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                print_safe("[Browser] Failed to connect to worker " + std::to_string(worker_id) + " output pipe");
                return false;
            }
        }

        print_safe("[Browser] Worker " + std::to_string(worker_id) + " connected");
        return true;
    }

    bool send_task(int worker_id, uint32_t task_id, const std::vector<uint8_t>& pixels) {
        WorkerInfo& worker = workers[worker_id];

        // Подготовка задачи
        Task task;
        task.type = 1;  // обычная задача
        task.task_id = task_id;
        task.data_size = static_cast<uint32_t>(pixels.size());

        DWORD bytes_written;

        // Отправляем заголовок задачи
        if (!WriteFile(worker.pipe_in, &task, sizeof(Task), &bytes_written, NULL)) {
            print_safe("[Browser] Failed to send task header to worker " + std::to_string(worker_id));
            return false;
        }

        // Отправляем данные пикселей
        if (!WriteFile(worker.pipe_in, pixels.data(), pixels.size(), &bytes_written, NULL)) {
            print_safe("[Browser] Failed to send task data to worker " + std::to_string(worker_id));
            return false;
        }

        print_safe("[Browser] Sent task " + std::to_string(task_id) +
            " to worker " + std::to_string(worker_id) +
            " (" + std::to_string(pixels.size()) + " pixels)");

        return true;
    }

    bool receive_result(int worker_id, uint32_t& task_id, std::vector<uint8_t>& result) {
        WorkerInfo& worker = workers[worker_id];

        // Читаем заголовок результата
        Result result_header;
        DWORD bytes_read;

        if (!ReadFile(worker.pipe_out, &result_header, sizeof(Result), &bytes_read, NULL)) {
            print_safe("[Browser] Failed to read result header from worker " + std::to_string(worker_id));
            return false;
        }

        task_id = result_header.task_id;
        result.resize(result_header.result_size);

        // Читаем данные результата
        if (!ReadFile(worker.pipe_out, result.data(), result.size(), &bytes_read, NULL)) {
            print_safe("[Browser] Failed to read result data from worker " + std::to_string(worker_id));
            return false;
        }

        print_safe("[Browser] Received result for task " + std::to_string(task_id) +
            " from worker " + std::to_string(worker_id));

        return true;
    }

    bool send_shutdown(int worker_id) {
        WorkerInfo& worker = workers[worker_id];

        Task shutdown_task;
        shutdown_task.type = 255;  // команда завершения
        shutdown_task.task_id = 0;
        shutdown_task.data_size = 0;

        DWORD bytes_written;
        if (!WriteFile(worker.pipe_in, &shutdown_task, sizeof(Task), &bytes_written, NULL)) {
            print_safe("[Browser] Failed to send shutdown to worker " + std::to_string(worker_id));
            return false;
        }

        print_safe("[Browser] Sent shutdown command to worker " + std::to_string(worker_id));
        return true;
    }

    void process_tasks(int total_tasks) {
        // Распределяем задачи по кругу между воркерами
        for (int i = 0; i < total_tasks; i++) {
            int worker_id = i % workers.size();

            // Генерируем случайные пиксели для задачи
            size_t pixel_count = 100 + (rand() % 901);  // 100-1000 пикселей
            auto pixels = generate_pixels(pixel_count);

            // Отправляем задачу
            if (!send_task(worker_id, i, pixels)) {
                break;
            }

            // Ждем результат (блокируемся в ожидании ответа)
            uint32_t received_task_id;
            std::vector<uint8_t> result;

            if (!receive_result(worker_id, received_task_id, result)) {
                break;
            }

            // Проверяем правильность результата (инверсия изображения)
            bool correct = true;
            for (size_t j = 0; j < pixels.size(); j++) {
                if (result[j] != (255 - pixels[j])) {
                    correct = false;
                    break;
                }
            }

            print_safe("[Browser] Task " + std::to_string(i) +
                " processed " + (correct ? "correctly" : "with errors"));

            // Небольшая пауза между задачами
            Sleep(500);
        }
    }

    void cleanup() {
        // Отправляем команды завершения всем воркерам
        for (size_t i = 0; i < workers.size(); i++) {
            if (workers[i].pipe_in != INVALID_HANDLE_VALUE) {
                send_shutdown(static_cast<int>(i));

                // Ждем завершения процесса
                WaitForSingleObject(workers[i].process_info.hProcess, 5000);
                TerminateProcess(workers[i].process_info.hProcess, 0);
                CloseHandle(workers[i].process_info.hProcess);
                CloseHandle(workers[i].process_info.hThread);

                // Закрываем дескрипторы каналов
                CloseHandle(workers[i].pipe_in);
                CloseHandle(workers[i].pipe_out);

                print_safe("[Browser] Worker " + std::to_string(i) + " terminated");
            }
        }
        workers.clear();
    }
};

int main() {
    // Создаем именованный мьютекс для синхронизации вывода
    console_mutex = CreateMutex(NULL, FALSE, "ConsoleOutputMutex");
    if (console_mutex == NULL) {
        std::cout << "Failed to create console mutex" << std::endl;
        return 1;
    }

    // Захватываем мьютекс для начального вывода
    WaitForSingleObject(console_mutex, INFINITE);
    std::cout << "=== Browser Process (Main Thread) ===" << std::endl;
    std::cout << "Simulates browser main thread with Web Workers" << std::endl;
    std::cout << "Worker task: Image inversion (pixel = 255 - pixel)" << std::endl;
    ReleaseMutex(console_mutex);

    int N, M;

    // Вывод без мьютекса для запроса ввода
    std::cout << "Enter number of workers (N): ";
    std::cin >> N;
    std::cout << "Enter number of tasks (M): ";
    std::cin >> M;

    if (N <= 0 || M <= 0) {
        print_safe("[Browser] Error: N and M must be positive numbers");
        CloseHandle(console_mutex);
        return 1;
    }

    Browser browser;

    // 1. Создаем каналы для каждого Worker
    print_safe("[Browser] Creating pipes for " + std::to_string(N) + " workers...");
    for (int i = 0; i < N; i++) {
        if (!browser.create_pipes_for_worker(i)) {
            print_safe("[Browser] Failed to create pipes for worker " + std::to_string(i));
            CloseHandle(console_mutex);
            return 1;
        }
    }

    // 2. Запускаем Worker процессы
    print_safe("[Browser] Starting worker processes...");
    for (int i = 0; i < N; i++) {
        if (!browser.start_worker(i)) {
            print_safe("[Browser] Failed to start worker " + std::to_string(i));
            CloseHandle(console_mutex);
            return 1;
        }
    }

    // 3. Распределяем задачи
    print_safe("[Browser] Distributing " + std::to_string(M) + " tasks...");
    browser.process_tasks(M);

    print_safe("[Browser] All tasks completed successfully");
    print_safe("=== Browser process finished ===");

    // Закрываем мьютекс
    CloseHandle(console_mutex);

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.ignore();
    std::cin.get();

    return 0;
}