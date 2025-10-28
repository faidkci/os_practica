#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <cstring>

// Используем все необходимые компоненты из std
using std::vector;
using std::string;
using std::unique_ptr;
using std::make_unique;
using std::runtime_error;
using std::invalid_argument;
using std::exception;
using std::cout;
using std::cin;
using std::cerr;
using std::endl;
using std::pair;
using std::make_pair;
using std::getline;
using std::numeric_limits;
using std::streamsize;
using std::to_string;
using std::strlen;
using std::strcmp;
using std::strcpy_s;

// Исключение для ошибок процессов
class ProcessException : public runtime_error {
    DWORD errorCode;
public:
    ProcessException(const string& msg, DWORD err)
        : runtime_error(msg + ", error: " + to_string(err) + " - " + GetSystemErrorString(err)),
        errorCode(err) {
    }

    DWORD getErrorCode() const { return errorCode; }

private:
    static string GetSystemErrorString(DWORD errorCode) {
        LPSTR messageBuffer = nullptr;
        DWORD size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&messageBuffer, 0, nullptr);

        string message;
        if (size > 0 && messageBuffer) {
            message = string(messageBuffer, size);
            while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
                message.pop_back();
            }
            LocalFree(messageBuffer);
        }
        else {
            message = "Unknown system error";
        }
        return message;
    }
};

// Вспомогательные функции для проверки операций
void CheckPipeOperation(bool success, const string& operationName) {
    if (!success) {
        throw ProcessException(operationName, GetLastError());
    }
}

template<typename T>
void CheckOperation(bool success, const string& operationName, T errorCode) {
    if (!success) {
        throw ProcessException(operationName, static_cast<DWORD>(errorCode));
    }
}

template<>
void CheckOperation(bool success, const string& operationName, BOOL) {
    if (!success) {
        throw ProcessException(operationName, GetLastError());
    }
}

// RAII-обертка для дескрипторов Windows
class HandleGuard {
public:
    explicit HandleGuard(HANDLE handle = nullptr) : handle_(handle) {}

    ~HandleGuard() {
        if (isValid()) {
            CloseHandle(handle_);
        }
    }

    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

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

    operator HANDLE() const { return handle_; }

private:
    HANDLE handle_;
};

// RAII-обертка для каналов (pipe)
class Pipe {
public:
    Pipe() = default;

    void create(SECURITY_ATTRIBUTES* sa = nullptr, DWORD size = 0) {
        HANDLE readEnd, writeEnd;
        CheckPipeOperation(CreatePipe(&readEnd, &writeEnd, sa, size), "CreatePipe");
        readEnd_.reset(readEnd);
        writeEnd_.reset(writeEnd);
    }

    HandleGuard& getReadEnd() { return readEnd_; }
    HandleGuard& getWriteEnd() { return writeEnd_; }

    void closeReadEnd() { readEnd_.reset(); }
    void closeWriteEnd() { writeEnd_.reset(); }

private:
    HandleGuard readEnd_;
    HandleGuard writeEnd_;
};

// Общие функции для безопасного ввода/вывода
namespace SafeIO {
    void readExact(HANDLE handle, void* buffer, DWORD size, const string& context) {
        BYTE* bytes = static_cast<BYTE*>(buffer);
        DWORD totalRead = 0;

        while (totalRead < size) {
            DWORD bytesRead = 0;
            CheckOperation(ReadFile(handle, bytes + totalRead, size - totalRead, &bytesRead, nullptr),
                context + " - ReadFile", TRUE);

            if (bytesRead == 0) {
                throw runtime_error(context + " - Unexpected end of file");
            }

            totalRead += bytesRead;
        }
    }

    void writeExact(HANDLE handle, const void* buffer, DWORD size, const string& context) {
        const BYTE* bytes = static_cast<const BYTE*>(buffer);
        DWORD totalWritten = 0;

        while (totalWritten < size) {
            DWORD bytesWritten = 0;
            CheckOperation(WriteFile(handle, bytes + totalWritten, size - totalWritten, &bytesWritten, nullptr),
                context + " - WriteFile", TRUE);
            totalWritten += bytesWritten;
        }
    }

    // Общие функции для работы с массивами
    void writeArray(HANDLE handle, const vector<int>& array, const string& context) {
        for (size_t i = 0; i < array.size(); ++i) {
            writeExact(handle, &array[i], sizeof(array[i]),
                context + " - writing array element " + to_string(i));
        }
    }

    vector<int> readArray(HANDLE handle, int size, const string& context) {
        vector<int> array(size);
        for (int i = 0; i < size; ++i) {
            readExact(handle, &array[i], sizeof(array[i]),
                context + " - reading array element " + to_string(i));
        }
        return array;
    }

    void writeSizeAndArray(HANDLE handle, int size, const vector<int>& array, const string& context) {
        writeExact(handle, &size, sizeof(size), context + " - writing array size");
        if (size > 0) {
            writeArray(handle, array, context);
        }
    }

    pair<int, vector<int>> readSizeAndArray(HANDLE handle, const string& context) {
        int size;
        readExact(handle, &size, sizeof(size), context + " - reading array size");

        if (size < 0) {
            throw invalid_argument("Invalid array size: " + to_string(size));
        }

        vector<int> array;
        if (size > 0) {
            array = readArray(handle, size, context);
        }

        return make_pair(size, array);
    }
}

// RAII-обертка для процесса
class Process {
public:
    Process() = default;

    ~Process() = default;

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    Process(Process&&) = default;
    Process& operator=(Process&&) = default;

    void create(LPCSTR application, LPSTR commandLine, STARTUPINFOA& si) {
        PROCESS_INFORMATION pi = {};

        unique_ptr<char[]> cmdLineCopy;
        if (commandLine) {
            size_t len = strlen(commandLine) + 1;
            cmdLineCopy = make_unique<char[]>(len);
            strcpy_s(cmdLineCopy.get(), len, commandLine);
        }

        CheckOperation(CreateProcessA(
            application,
            cmdLineCopy ? cmdLineCopy.get() : nullptr,
            nullptr, nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si,
            &pi
        ), "CreateProcess", TRUE);

        process_.reset(pi.hProcess);
        thread_.reset(pi.hThread);
    }

    HANDLE getProcessHandle() const { return process_.get(); }

    bool isRunning() const {
        if (!process_.isValid()) return false;

        DWORD exitCode;
        if (!GetExitCodeProcess(process_.get(), &exitCode)) {
            return false;
        }
        return exitCode == STILL_ACTIVE;
    }

    DWORD wait(DWORD timeout = INFINITE) const {
        DWORD result = WaitForSingleObject(process_.get(), timeout);
        if (result == WAIT_FAILED) {
            throw ProcessException("WaitForSingleObject", GetLastError());
        }
        return result;
    }

    DWORD getExitCode() const {
        DWORD exitCode;
        CheckOperation(GetExitCodeProcess(process_.get(), &exitCode), "GetExitCodeProcess", TRUE);
        return exitCode;
    }

    void terminate(UINT exitCode = 1) {
        if (isRunning()) {
            CheckOperation(TerminateProcess(process_.get(), exitCode), "TerminateProcess", TRUE);
        }
    }

private:
    HandleGuard process_;
    HandleGuard thread_;
};

// Общие функции для работы с вводом
namespace InputUtils {
    void ClearInputBuffer() {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    int SafeInputInt(const string& prompt) {
        int value;
        while (true) {
            cout << prompt;
            if (!(cin >> value)) {
                cout << "Invalid input. Please enter a valid integer.\n";
                ClearInputBuffer();
            }
            else {
                ClearInputBuffer();
                break;
            }
        }
        return value;
    }

    vector<int> InputArrayWithValidation(int size) {
        vector<int> array;
        array.reserve(size);

        cout << "Enter " << size << " elements:\n";

        for (int i = 0; i < size; ++i) {
            string prompt = "Element " + to_string(i + 1) + ": ";
            int element = SafeInputInt(prompt);
            array.push_back(element);
        }

        cout << "Checking for extra input... ";
        string extra;
        if (getline(cin, extra)) {
            if (!extra.empty()) {
                throw runtime_error("Too many elements provided. Expected " +
                    to_string(size) + " elements.");
            }
        }

        return array;
    }

    pair<int, vector<int>> GetArrayFromUser() {
        int size = SafeInputInt("Enter array size: ");
        if (size <= 0) {
            throw invalid_argument("Array size must be greater than 0");
        }
        vector<int> array = InputArrayWithValidation(size);
        return make_pair(size, array);
    }
}

// Общие функции для работы с процессами
namespace ProcessUtils {
    unique_ptr<char[]> PrepareCommandLine(const string& commandLine) {
        unique_ptr<char[]> cmdLineCopy = make_unique<char[]>(commandLine.size() + 1);
        strcpy_s(cmdLineCopy.get(), commandLine.size() + 1, commandLine.c_str());
        return cmdLineCopy;
    }

    string GetSelfPath() {
        char selfPath[MAX_PATH];
        CheckOperation(GetModuleFileNameA(nullptr, selfPath, MAX_PATH),
            "GetModuleFileName", TRUE);
        return string(selfPath);
    }

    Process CreateChildProcess(const string& commandLine, STARTUPINFOA& si) {
        Process childProcess;
        auto cmdLineCopy = PrepareCommandLine(commandLine);
        childProcess.create(nullptr, cmdLineCopy.get(), si);
        return childProcess;
    }
}

// Функция для работы в режиме потомка
void ChildMode() {
    HandleGuard hStdIn(GetStdHandle(STD_INPUT_HANDLE));
    HandleGuard hStdOut(GetStdHandle(STD_OUTPUT_HANDLE));

    if (!hStdIn.isValid() || !hStdOut.isValid()) {
        throw runtime_error("Invalid standard handles in child process");
    }

    auto data = SafeIO::readSizeAndArray(hStdIn.get(), "child process");
    int size = data.first;
    vector<int> array = data.second;

    if (size == 0) {
        int result = -1;
        SafeIO::writeExact(hStdOut.get(), &result, sizeof(result), "writing empty array result");
        return;
    }

    int maxElement = array[0];
    for (size_t i = 1; i < array.size(); ++i) {
        if (array[i] > maxElement) {
            maxElement = array[i];
        }
    }

    SafeIO::writeExact(hStdOut.get(), &maxElement, sizeof(maxElement), "writing result");
}

// Функция для работы в режиме родителя
void ParentMode() {
    Pipe stdinPipe, stdoutPipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    stdinPipe.create(&sa, 0);
    stdoutPipe.create(&sa, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinPipe.getReadEnd().get();
    si.hStdOutput = stdoutPipe.getWriteEnd().get();
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    string selfPath = ProcessUtils::GetSelfPath();
    Process childProcess = ProcessUtils::CreateChildProcess(selfPath + " child", si);

    stdinPipe.closeReadEnd();
    stdoutPipe.closeWriteEnd();

    try {
        auto data = InputUtils::GetArrayFromUser();
        int size = data.first;
        vector<int> array = data.second;

        SafeIO::writeSizeAndArray(stdinPipe.getWriteEnd().get(), size, array, "parent process");
        stdinPipe.closeWriteEnd();

        int result;
        const DWORD timeoutMs = 5000;

        DWORD waitResult = childProcess.wait(timeoutMs);
        if (waitResult == WAIT_OBJECT_0) {
            DWORD exitCode = childProcess.getExitCode();
            if (exitCode != 0) {
                throw runtime_error("Child process failed with exit code: " + to_string(exitCode));
            }

            SafeIO::readExact(stdoutPipe.getReadEnd().get(), &result, sizeof(result), "reading result");

            if (result == -1) {
                cout << "Child reported: Empty array" << endl;
            }
            else {
                cout << "Max element: " << result << endl;
            }
        }
        else if (waitResult == WAIT_TIMEOUT) {
            childProcess.terminate();
            throw runtime_error("Child process timeout after " + to_string(timeoutMs) + " ms");
        }
        else {
            childProcess.terminate();
            throw runtime_error("Unexpected wait result: " + to_string(waitResult));
        }
    }
    catch (const exception& e) {
        childProcess.terminate();
        throw;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && strcmp(argv[1], "child") == 0) {
            ChildMode();
        }
        else {
            ParentMode();
        }
        return 0;
    }
    catch (const ProcessException& e) {
        cerr << "Process error: " << e.what() << endl;
        return static_cast<int>(e.getErrorCode());
    }
    catch (const invalid_argument& e) {
        cerr << "Input error: " << e.what() << endl;
        return 1;
    }
    catch (const runtime_error& e) {
        cerr << "Runtime error: " << e.what() << endl;
        return 1;
    }
    catch (const exception& e) {
        cerr << "Unexpected exception: " << e.what() << endl;
        return 1;
    }
    catch (...) {
        cerr << "Unknown exception occurred" << endl;
        return 1;
    }
}
