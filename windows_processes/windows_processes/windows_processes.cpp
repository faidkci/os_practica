#include <windows.h>
#include <iostream>
#include <vector>

using namespace std;

// Функция для работы в режиме потомка
void ChildMode() {
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    
    int size;
    DWORD bytesRead;
    
    // Чтение размера массива из stdin
    if (!ReadFile(hStdIn, &size, sizeof(size), &bytesRead, NULL) || bytesRead != sizeof(size)) {
        cerr << "Child: Failed to read array size" << endl;
        return;
    }

    vector<int> array(size);
    // Чтение элементов массива
    for (int i = 0; i < size; ++i) {
        if (!ReadFile(hStdIn, &array[i], sizeof(array[i]), &bytesRead, NULL) || bytesRead != sizeof(array[i])) {
            cerr << "Child: Failed to read array element" << endl;
            return;
        }
    }

    // Поиск максимального элемента
    if (array.empty()) {
        // Отправляем специальное значение для пустого массива
        int errorResult = -1;
        DWORD bytesWritten;
        WriteFile(hStdOut, &errorResult, sizeof(errorResult), &bytesWritten, NULL);
        cerr << "Child: Empty array" << endl;
        return;
    }

    int maxElement = array[0];
    for (int i = 1; i < array.size(); ++i) {
        if (array[i] > maxElement) maxElement = array[i];
    }

    // Запись результата в stdout
    DWORD bytesWritten;
    if (!WriteFile(hStdOut, &maxElement, sizeof(maxElement), &bytesWritten, NULL)) {
        cerr << "Child: Failed to write result" << endl;
    }
}

// Функция для работы в режиме родителя
void ParentMode() {
    // Создание каналов
    HANDLE hStdInRead, hStdInWrite;
    HANDLE hStdOutRead, hStdOutWrite;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
        cerr << "Parent: Failed to create stdin pipe" << endl;
        return;
    }

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        cerr << "Parent: Failed to create stdout pipe" << endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        return;
    }

    // Подготовка структур для запуска процесса
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdInRead;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    // Получение пути к исполняемому файлу
    char selfPath[MAX_PATH];
    GetModuleFileNameA(NULL, selfPath, MAX_PATH);

    // Формирование командной строки с использованием char*
    char commandLine[MAX_PATH + 10];
    strcpy_s(commandLine, selfPath);
    strcat_s(commandLine, " child");

    // Запуск дочернего процесса
    if (!CreateProcessA(
        NULL,
        commandLine,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        cerr << "Parent: Failed to create process" << endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return;
    }

    // Закрываем ненужные дескрипторы в родительском процессе
    CloseHandle(hStdInRead);
    CloseHandle(hStdOutWrite);
    CloseHandle(pi.hThread); // Закрываем handle потока сразу

    // Ввод данных массива
    int size;
    cout << "Enter array size: ";
    cin >> size;

    if (size <= 0) {
        cerr << "Parent: Invalid array size" << endl;
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(pi.hProcess);
        return;
    }

    vector<int> array(size);
    cout << "Enter " << size << " elements:\n";
    for (int i = 0; i < size; ++i) {
        cin >> array[i];
    }

    // Отправка данных в дочерний процесс
    DWORD bytesWritten;
    if (!WriteFile(hStdInWrite, &size, sizeof(size), &bytesWritten, NULL)) {
        cerr << "Parent: Failed to write size" << endl;
    }

    for (int i = 0; i < size; ++i) {
        if (!WriteFile(hStdInWrite, &array[i], sizeof(array[i]), &bytesWritten, NULL)) {
            cerr << "Parent: Failed to write array element" << endl;
            break;
        }
    }

    // Закрываем дескриптор записи в stdin дочернего процесса
    // Это сигнализирует конец данных
    CloseHandle(hStdInWrite);

    // Чтение результата из дочернего процесса
    int result;
    DWORD bytesRead;
    
    // Ждем данные с таймаутом (5000 мс)
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 5000);
    if (waitResult == WAIT_OBJECT_0) {
        // Процесс завершился, пробуем прочитать если что-то осталось в буфере
        if (ReadFile(hStdOutRead, &result, sizeof(result), &bytesRead, NULL) && bytesRead == sizeof(result)) {
            if (result == -1) {
                cout << "Child reported: Empty array" << endl;
            } else {
                cout << "Max element: " << result << endl;
            }
        } else {
            // Пробуем получить код возврата
            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
                cerr << "Parent: Child process exited with code " << exitCode << ", but no result received" << endl;
            } else {
                cerr << "Parent: Failed to read result and get exit code" << endl;
            }
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        // Таймаут - процесс завис
        cerr << "Parent: Child process timeout" << endl;
        TerminateProcess(pi.hProcess, 1);
    } else {
        // Ошибка ожидания
        cerr << "Parent: Wait for child process failed" << endl;
    }

    // Закрытие дескрипторов
    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
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
