#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <thread>

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

void print_safe(int worker_id, const std::string& message) {
    if (console_mutex) {
        WaitForSingleObject(console_mutex, INFINITE);
    }
    std::cout << "[Worker " << worker_id << "] " << message << std::endl;
    if (console_mutex) {
        ReleaseMutex(console_mutex);
    }
}

// Функция инверсии изображения (вариант 14)
std::vector<uint8_t> invert_image(const std::vector<uint8_t>& pixels) {
    std::vector<uint8_t> result(pixels.size());
    for (size_t i = 0; i < pixels.size(); i++) {
        result[i] = 255 - pixels[i];
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: Worker.exe <worker_id>" << std::endl;
        return 1;
    }

    int worker_id = std::stoi(argv[1]);

    // Открываем существующий именованный мьютекс
    console_mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, "ConsoleOutputMutex");
    if (console_mutex == NULL) {
        // Если мьютекс не существует, создаем его
        console_mutex = CreateMutex(NULL, FALSE, "ConsoleOutputMutex");
        if (console_mutex == NULL) {
            std::cout << "[Worker " << worker_id << "] Failed to create/open console mutex" << std::endl;
            return 1;
        }
    }

    print_safe(worker_id, "Starting...");
    print_safe(worker_id, "Task: Image inversion (pixel = 255 - pixel)");

    // Формируем имена каналов
    std::string pipe_in_name = "\\\\.\\pipe\\worker_in_" + std::to_string(worker_id);
    std::string pipe_out_name = "\\\\.\\pipe\\worker_out_" + std::to_string(worker_id);

    print_safe(worker_id, "Connecting to input pipe: " + pipe_in_name);

    // Подключаемся к входному каналу (для получения задач)
    HANDLE pipe_in = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 10; attempt++) {
        pipe_in = CreateFile(
            pipe_in_name.c_str(),
            GENERIC_READ,
            0, NULL, OPEN_EXISTING, 0, NULL
        );

        if (pipe_in != INVALID_HANDLE_VALUE) {
            break;
        }

        if (GetLastError() == ERROR_PIPE_BUSY) {
            if (!WaitNamedPipe(pipe_in_name.c_str(), 5000)) {
                print_safe(worker_id, "Timeout waiting for input pipe");
                CloseHandle(console_mutex);
                return 1;
            }
        }

        Sleep(500);
    }

    if (pipe_in == INVALID_HANDLE_VALUE) {
        print_safe(worker_id, "Failed to connect to input pipe");
        CloseHandle(console_mutex);
        return 1;
    }

    print_safe(worker_id, "Connecting to output pipe: " + pipe_out_name);

    // Подключаемся к выходному каналу (для отправки результатов)
    HANDLE pipe_out = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 10; attempt++) {
        pipe_out = CreateFile(
            pipe_out_name.c_str(),
            GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL
        );

        if (pipe_out != INVALID_HANDLE_VALUE) {
            break;
        }

        if (GetLastError() == ERROR_PIPE_BUSY) {
            if (!WaitNamedPipe(pipe_out_name.c_str(), 5000)) {
                print_safe(worker_id, "Timeout waiting for output pipe");
                CloseHandle(pipe_in);
                CloseHandle(console_mutex);
                return 1;
            }
        }

        Sleep(500);
    }

    if (pipe_out == INVALID_HANDLE_VALUE) {
        print_safe(worker_id, "Failed to connect to output pipe");
        CloseHandle(pipe_in);
        CloseHandle(console_mutex);
        return 1;
    }

    print_safe(worker_id, "Connected to Browser. Waiting for tasks...");

    // Основной рабочий цикл
    bool running = true;
    while (running) {
        // Читаем заголовок задачи
        Task task;
        DWORD bytes_read;

        if (!ReadFile(pipe_in, &task, sizeof(Task), &bytes_read, NULL)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                print_safe(worker_id, "Pipe broken, exiting");
                break;
            }
            continue;
        }

        // Проверяем тип задачи
        if (task.type == 255) {  // Команда завершения
            print_safe(worker_id, "Received shutdown command");
            break;
        }

        if (task.type != 1) {
            print_safe(worker_id, "Unknown task type: " + std::to_string(task.type));
            continue;
        }

        print_safe(worker_id, "Received task " + std::to_string(task.task_id) +
            " (" + std::to_string(task.data_size) + " pixels)");

        // Читаем данные пикселей
        std::vector<uint8_t> pixels(task.data_size);
        if (!ReadFile(pipe_in, pixels.data(), task.data_size, &bytes_read, NULL)) {
            print_safe(worker_id, "Failed to read pixel data");
            break;
        }

        // Имитируем "тяжелую" работу
        print_safe(worker_id, "Processing task " + std::to_string(task.task_id) + "...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Выполняем инверсию изображения (вариант 14)
        auto result_pixels = invert_image(pixels);

        // Отправляем результат
        Result result;
        result.task_id = task.task_id;
        result.result_size = static_cast<uint32_t>(result_pixels.size());

        DWORD bytes_written;
        if (!WriteFile(pipe_out, &result, sizeof(Result), &bytes_written, NULL)) {
            print_safe(worker_id, "Failed to send result header");
            break;
        }

        if (!WriteFile(pipe_out, result_pixels.data(), result_pixels.size(), &bytes_written, NULL)) {
            print_safe(worker_id, "Failed to send result data");
            break;
        }

        print_safe(worker_id, "Task " + std::to_string(task.task_id) + " completed");
    }

    print_safe(worker_id, "Shutting down...");

    // Закрываем дескрипторы
    if (pipe_in != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_in);
    }

    if (pipe_out != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_out);
    }

    print_safe(worker_id, "Worker process finished");

    // Закрываем мьютекс
    CloseHandle(console_mutex);

    return 0;
}