#include <iostream>
#include <cctype>
#include <string>
#include <windows.h>
#include <process.h>

using namespace std;

// Структура для передачи данных в поток
struct ThreadData {
    const char* inputString; // Входная строка
    char* result;            // Результат (знаки препинания и разделители)
};

// Функция потока worker для CreateThread
DWORD WINAPI workerThread(LPVOID param) {
    ThreadData* data = (ThreadData*)param;
    const char* str = data->inputString;
    size_t len = strlen(str);
    char* temp = new char[len + 1]; // Временный буфер
    int index = 0;

    // Извлекаем знаки препинания и разделители
    for (size_t i = 0; i < len; i++) {
        if (ispunct(static_cast<unsigned char>(str[i]))) {
            temp[index++] = str[i];
        }
    }
    temp[index] = '\0';

    // Сохраняем результат в структуре
    data->result = new char[index + 1];
    strcpy_s(data->result, index + 1, temp); 
    delete[] temp;

    return 0;
}

// Функция потока worker для _beginthreadex
unsigned __stdcall workerThreadEx(void* param) {
    return (unsigned)workerThread(param);
}

int main() {
    // Ввод строки любой длины
    string input;
    cout << "Enter a string: ";
    getline(cin, input);

    // Ввод задержки
    int sleepTime;
    cout << "Enter sleep time (ms): ";
    cin >> sleepTime;

    // Подготовка данных для потока
    ThreadData data;
    data.inputString = input.c_str(); // передаём указатель на данные строки
    data.result = nullptr;

    // Создание потока в приостановленном состоянии
    HANDLE hThread = CreateThread(
        NULL,
        0,
        workerThread,
        &data,
        CREATE_SUSPENDED,
        NULL
    );

    if (hThread == NULL) {
        cerr << "Thread creation failed. Error: " << GetLastError() << endl;
        return 1;
    }

    cout << "Thread created in suspended state. Waiting for " << sleepTime << " ms..." << endl;

    // Ожидание указанное время
    Sleep(sleepTime);

    // Запуск потока
    if (ResumeThread(hThread) == (DWORD)-1) {
        cerr << "ResumeThread failed. Error: " << GetLastError() << endl;
        CloseHandle(hThread);
        return 1;
    }

    cout << "Thread resumed. Waiting for completion..." << endl;

    // Ожидание завершения потока
    DWORD waitResult = WaitForSingleObject(hThread, INFINITE);
    if (waitResult == WAIT_FAILED) {
        cerr << "WaitForSingleObject failed. Error: " << GetLastError() << endl;
        CloseHandle(hThread);
        return 1;
    }

    // Вывод результата
    if (data.result != nullptr && strlen(data.result) > 0) {
        cout << "Punctuation and separator characters: " << data.result << endl;
        delete[] data.result;
    } else {
        cout << "No punctuation or separator characters found." << endl;
    }

    // Закрытие дескриптора потока
    CloseHandle(hThread);

    cout << "Press Enter to exit...";
    cin.ignore();
    cin.get();

    return 0;
}
