#include <windows.h>
#include <iostream>
#include <vector>

using namespace std;

// Функция для работы в режиме потомка
void ChildMode() {
    int size;
    // Чтение размера массива из stdin
    if (!cin.read(reinterpret_cast<char*>(&size), sizeof(size))) {
        cerr << "Child: Failed to read array size" << endl;
        return;
    }

    vector<int> array(size);
    // Чтение элементов массива
    for (int i = 0; i < size; ++i) {
        if (!cin.read(reinterpret_cast<char*>(&array[i]), sizeof(array[i]))) {
            cerr << "Child: Failed to read array element" << endl;
            return;
        }
    }

    // Поиск минимального элемента
    if (array.empty()) {
        cerr << "Child: Empty array" << endl;
        return;
    }

    int minElement = array[0];
    for (int i = 0; i < array.size(); ++i) {
        if (array[i] < minElement) minElement = array[i];
    }

    // Запись результата в stdout
    if (!cout.write(reinterpret_cast<char*>(&minElement), sizeof(minElement))) {
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
    char commandLine[MAX_PATH + 10]; // +10 для " child" и запаса
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

    // Ввод данных массива
    int size;
    cout << "Enter array size: ";
    cin >> size;

    if (size <= 0) {
        cerr << "Parent: Invalid array size" << endl;
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
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
    CloseHandle(hStdInWrite);

    // Чтение результата из дочернего процесса
    int result;
    DWORD bytesRead;
    if (ReadFile(hStdOutRead, &result, sizeof(result), &bytesRead, NULL)) {
        cout << "Min element: " << result << endl;
    }
    else {
        cerr << "Parent: Failed to read result" << endl;
    }

    // Ожидание завершения и закрытие дескрипторов
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
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