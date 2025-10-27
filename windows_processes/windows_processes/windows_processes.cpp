#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>

using namespace std;

// RAII-обертка для дескрипторов Windows
class HandleGuard {
public:
    explicit HandleGuard(HANDLE handle = nullptr) : handle_(handle) {}

    ~HandleGuard() {
        if (isValid()) {
            CloseHandle(handle_);
        }
    }

    // Запрещаем копирование
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    // Разрешаем перемещение
    HandleGuard(HandleGuard&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    HANDLE get() const { return handle_; }
    bool isValid() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

    void reset(HANDLE newHandle = nullptr) {
        if (isValid()) {
            CloseHandle(handle_);
        }
        handle_ = newHandle;
    }

    HANDLE release() {
        HANDLE temp = handle_;
        handle_ = nullptr;
        return temp;
    }

    // Оператор приведения к HANDLE для удобства
    operator HANDLE() const { return handle_; }

private:
    HANDLE handle_;
};

// RAII-обертка для каналов (pipe)
class Pipe {
public:
    Pipe() = default;

    bool create(SECURITY_ATTRIBUTES* sa = nullptr, DWORD size = 0) {
        HANDLE readEnd, writeEnd;
        if (!CreatePipe(&readEnd, &writeEnd, sa, size)) {
            return false;
        }
        readEnd_.reset(readEnd);
        writeEnd_.reset(writeEnd);
        return readEnd_.isValid() && writeEnd_.isValid();
    }

    HandleGuard& getReadEnd() { return readEnd_; }
    HandleGuard& getWriteEnd() { return writeEnd_; }

    void closeReadEnd() { readEnd_.reset(); }
    void closeWriteEnd() { writeEnd_.reset(); }

private:
    HandleGuard readEnd_;
    HandleGuard writeEnd_;
};

// RAII-обертка для процесса
class Process {
public:
    Process() = default;

    ~Process() {
        // Деструктор автоматически закроет handle'ы
    }

    // Запрещаем копирование
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // Разрешаем перемещение
    Process(Process&&) = default;
    Process& operator=(Process&&) = default;

    bool create(LPCSTR application, LPSTR commandLine, STARTUPINFOA& si) {
        PROCESS_INFORMATION pi = {};
        if (!CreateProcessA(application, commandLine, nullptr, nullptr, TRUE, 0,
            nullptr, nullptr, &si, &pi)) {
            return false;
        }

        process_.reset(pi.hProcess);
        thread_.reset(pi.hThread);
        return true;
    }

    HANDLE getProcessHandle() const { return process_.get(); }
    bool isRunning() const { return process_.isValid(); }

    DWORD wait(DWORD timeout = INFINITE) const {
        return WaitForSingleObject(process_.get(), timeout);
    }

    bool getExitCode(DWORD& exitCode) const {
        return GetExitCodeProcess(process_.get(), &exitCode);
    }

    void terminate(UINT exitCode = 1) {
        if (isRunning()) {
            TerminateProcess(process_.get(), exitCode);
        }
    }

private:
    HandleGuard process_;
    HandleGuard thread_;
};

// Функция для работы в режиме потомка с RAII
void ChildMode() {
    HandleGuard hStdIn(GetStdHandle(STD_INPUT_HANDLE));
    HandleGuard hStdOut(GetStdHandle(STD_OUTPUT_HANDLE));

    int size;
    DWORD bytesRead;

    // Чтение размера массива из stdin
    if (!ReadFile(hStdIn.get(), &size, sizeof(size), &bytesRead, nullptr) ||
        bytesRead != sizeof(size)) {
        cerr << "Child: Failed to read array size" << endl;
        return;
    }

    vector<int> array(size);
    // Чтение элементов массива
    for (int i = 0; i < size; ++i) {
        if (!ReadFile(hStdIn.get(), &array[i], sizeof(array[i]), &bytesRead, nullptr) ||
            bytesRead != sizeof(array[i])) {
            cerr << "Child: Failed to read array element" << endl;
            return;
        }
    }

    // Поиск максимального элемента
    if (array.empty()) {
        // Отправляем специальное значение для пустого массива
        int errorResult = -1;
        DWORD bytesWritten;
        WriteFile(hStdOut.get(), &errorResult, sizeof(errorResult), &bytesWritten, nullptr);
        cerr << "Child: Empty array" << endl;
        return;
    }

    int maxElement = array[0];
    for (size_t i = 1; i < array.size(); ++i) {
        if (array[i] > maxElement) maxElement = array[i];
    }

    // Запись результата в stdout
    DWORD bytesWritten;
    if (!WriteFile(hStdOut.get(), &maxElement, sizeof(maxElement), &bytesWritten, nullptr)) {
        cerr << "Child: Failed to write result" << endl;
    }
}

// Функция для работы в режиме родителя с RAII
void ParentMode() {
    // Создание каналов
    Pipe stdinPipe, stdoutPipe;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!stdinPipe.create(&sa, 0)) {
        cerr << "Parent: Failed to create stdin pipe" << endl;
        return;
    }

    if (!stdoutPipe.create(&sa, 0)) {
        cerr << "Parent: Failed to create stdout pipe" << endl;
        return;
    }

    // Подготовка структур для запуска процесса
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    Process childProcess;

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinPipe.getReadEnd().get();
    si.hStdOutput = stdoutPipe.getWriteEnd().get();
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    // Получение пути к исполняемому файлу
    char selfPath[MAX_PATH];
    GetModuleFileNameA(nullptr, selfPath, MAX_PATH);

    // Формирование командной строки с использованием char[]
    char commandLine[MAX_PATH + 10];
    strcpy_s(commandLine, selfPath);
    strcat_s(commandLine, " child");

    // Запуск дочернего процесса
    if (!childProcess.create(nullptr, commandLine, si)) {
        cerr << "Parent: Failed to create process" << endl;
        return;
    }

    // Закрываем ненужные дескрипторы в родительском процессе
    stdinPipe.closeReadEnd();   // Закрываем read end stdin pipe (используется дочерним процессом)
    stdoutPipe.closeWriteEnd(); // Закрываем write end stdout pipe (используется дочерним процессом)

    // Ввод данных массива
    int size;
    cout << "Enter array size: ";
    cin >> size;

    if (size <= 0) {
        cerr << "Parent: Invalid array size" << endl;
        return;
    }

    vector<int> array(size);
    cout << "Enter " << size << " elements:\n";
    for (int i = 0; i < size; ++i) {
        cin >> array[i];
    }

    // Отправка данных в дочерний процесс
    DWORD bytesWritten;
    if (!WriteFile(stdinPipe.getWriteEnd().get(), &size, sizeof(size), &bytesWritten, nullptr)) {
        cerr << "Parent: Failed to write size" << endl;
    }

    for (int i = 0; i < size; ++i) {
        if (!WriteFile(stdinPipe.getWriteEnd().get(), &array[i], sizeof(array[i]), &bytesWritten, nullptr)) {
            cerr << "Parent: Failed to write array element" << endl;
            break;
        }
    }

    // Закрываем дескриптор записи в stdin дочернего процесса
    // Это сигнализирует конец данных
    stdinPipe.closeWriteEnd();

    // Чтение результата из дочернего процесса
    int result;
    DWORD bytesRead;

    // Ждем данные с таймаутом (5000 мс)
    DWORD waitResult = childProcess.wait(5000);
    if (waitResult == WAIT_OBJECT_0) {
        // Процесс завершился, пробуем прочитать если что-то осталось в буфере
        if (ReadFile(stdoutPipe.getReadEnd().get(), &result, sizeof(result), &bytesRead, nullptr) &&
            bytesRead == sizeof(result)) {
            if (result == -1) {
                cout << "Child reported: Empty array" << endl;
            }
            else {
                cout << "Max element: " << result << endl;
            }
        }
        else {
            // Пробуем получить код возврата
            DWORD exitCode;
            if (childProcess.getExitCode(exitCode)) {
                cerr << "Parent: Child process exited with code " << exitCode << ", but no result received" << endl;
            }
            else {
                cerr << "Parent: Failed to read result and get exit code" << endl;
            }
        }
    }
    else if (waitResult == WAIT_TIMEOUT) {
        // Таймаут - процесс завис
        cerr << "Parent: Child process timeout" << endl;
        childProcess.terminate(1);
    }
    else {
        // Ошибка ожидания
        cerr << "Parent: Wait for child process failed" << endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        ChildMode();
    }
    else {
        ParentMode();
    }
    return 0;
}
