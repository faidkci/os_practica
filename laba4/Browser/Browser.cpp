#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <thread>
#include <sstream>

// Функция для вывода разделительной линии
void printSeparator(int length = 60) {
    std::cout << std::string(length, '=') << std::endl;
}

// Функция для вывода заголовка
void printHeader() {
    std::cout << "\n";
    printSeparator();
    std::cout << "           МЕНЕДЖЕР ЗАГРУЗОК БРАУЗЕРА" << std::endl;
    std::cout << "  Лабораторная работа №4: Синхронизация процессов" << std::endl;
    printSeparator();
}

// Функция для вывода статистики
void printStatistics(int totalFiles, int maxSimultaneous, int startedProcesses) {
    std::cout << "\n";
    printSeparator();
    std::cout << "                     СТАТИСТИКА ВЫПОЛНЕНИЯ" << std::endl;
    printSeparator();
    std::cout << "  Всего файлов для загрузки:       " << std::setw(4) << totalFiles << std::endl;
    std::cout << "  Максимум одновременных загрузок: " << std::setw(4) << maxSimultaneous << std::endl;
    std::cout << "  Успешно запущено процессов:      " << std::setw(4) << startedProcesses << std::endl;
    printSeparator();
}

int main() {
    setlocale(LC_ALL, "RUS");
    int N, M;

    // Выводим заголовок
    printHeader();

    // 1. Запрос параметров у пользователя
    std::cout << "\nВведите параметры загрузки:\n";
    std::cout << "Максимальное количество одновременных загрузок (N): ";
    std::cin >> N;
    std::cout << "Общее количество файлов для загрузки (M > N): ";
    std::cin >> M;

    // Проверка корректности введенных данных
    if (M <= N) {
        std::cerr << "\nОШИБКА: M должно быть больше N!" << std::endl;
        std::cerr << "Завершение работы программы." << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "\n";
    printSeparator();
    std::cout << "СОЗДАНИЕ ОБЪЕКТОВ СИНХРОНИЗАЦИИ" << std::endl;
    printSeparator();

    // 2. Создание объектов синхронизации
    HANDLE hSemaphore = CreateSemaphoreA(
        NULL,
        N,
        N,
        "DownloadSlots"
    );

    HANDLE hMutex = CreateMutexA(
        NULL,
        FALSE,
        "LogAccessMutex"
    );

    HANDLE hEvent = CreateEventA(
        NULL,
        TRUE,
        FALSE,
        "BrowserClosingEvent"
    );

    // Проверка успешности создания объектов синхронизации
    if (!hSemaphore || !hMutex || !hEvent) {
        DWORD error = GetLastError();
        std::cerr << "\nОШИБКА: Не удалось создать объекты синхронизации!" << std::endl;
        std::cerr << "Код ошибки: " << error << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "  ✓ Семафор 'DownloadSlots' создан (" << N << " слотов)" << std::endl;
    std::cout << "  ✓ Мьютекс 'LogAccessMutex' создан" << std::endl;
    std::cout << "  ✓ Событие 'BrowserClosingEvent' создано" << std::endl;

    std::cout << "\n";
    printSeparator();
    std::cout << "ЗАПУСК ПРОЦЕССОВ ЗАГРУЗКИ" << std::endl;
    printSeparator();

    // 3. Запуск M процессов Downloader.exe
    std::vector<PROCESS_INFORMATION> procInfos;
    std::vector<HANDLE> processHandles;
    int startedCount = 0;

    std::cout << "\nЗапуск " << M << " процессов загрузки..." << std::endl;
    std::cout << "Активные загрузки (первые " << N << " процессов):\n" << std::endl;

    for (int i = 0; i < M; ++i) {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        // Формируем командную строку с номером варианта (от 1 до 9)
        int variant = (i % 9) + 1;
        std::string commandLine = "Downloader.exe " + std::to_string(variant);

        // Создаем копию строки для CreateProcessA
        char* cmdLine = new char[commandLine.length() + 1];
        strcpy_s(cmdLine, commandLine.length() + 1, commandLine.c_str());

        // Запускаем процесс
        if (!CreateProcessA(
            NULL,
            cmdLine,
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &si,
            &pi
        )) {
            DWORD error = GetLastError();
            std::cerr << "  ✗ Ошибка запуска процесса " << i + 1
                << " (вариант " << variant << ")" << std::endl;
            std::cerr << "  Код ошибки: " << error << std::endl;
        }
        else {
            procInfos.push_back(pi);
            processHandles.push_back(pi.hProcess);
            startedCount++;

            std::cout << "  ✓ Процесс " << std::setw(2) << i + 1
                << " запущен (PID: " << std::setw(6) << pi.dwProcessId
                << ", вариант: " << variant << ")" << std::endl;
        }

        delete[] cmdLine;

        // Небольшая задержка между запуском процессов
        Sleep(100);
    }

    std::cout << "\nВсего запущено процессов: " << startedCount << " из " << M << std::endl;

    std::cout << "\n";
    printSeparator();
    std::cout << "БРАУЗЕР ЗАПУЩЕН" << std::endl;
    std::cout << "Состояние: Ожидание завершения загрузок" << std::endl;
    std::cout << "\nНажмите Enter для закрытия браузера..." << std::endl;
    printSeparator();

    // 4. Ожидание нажатия Enter
    std::cout << "\n> ";
    std::cin.ignore();
    std::cin.get();

    // 5. Сигнал о закрытии браузера
    std::cout << "\n";
    printSeparator();
    std::cout << "ЗАКРЫТИЕ БРАУЗЕРА" << std::endl;
    printSeparator();

    std::cout << "Отправка сигнала завершения всем загрузкам..." << std::endl;
    SetEvent(hEvent);

    // 6. Ожидание завершения всех процессов
    if (!processHandles.empty()) {
        std::cout << "\nОжидание завершения " << processHandles.size()
            << " процессов (максимум 30 секунд)..." << std::endl;

        DWORD waitResult = WaitForMultipleObjects(
            (DWORD)processHandles.size(),
            processHandles.data(),
            TRUE,
            30000
        );

        if (waitResult == WAIT_TIMEOUT) {
            std::cout << "\nПРЕДУПРЕЖДЕНИЕ: Таймаут ожидания!" << std::endl;
            std::cout << "Не все процессы завершились за отведенное время." << std::endl;
            std::cout << "Принудительное завершение оставшихся процессов..." << std::endl;

            // Принудительно завершаем процессы, которые еще работают
            for (size_t i = 0; i < procInfos.size(); i++) {
                DWORD exitCode;
                if (GetExitCodeProcess(processHandles[i], &exitCode) && exitCode == STILL_ACTIVE) {
                    std::cout << "  Принудительное завершение процесса PID: "
                        << procInfos[i].dwProcessId << std::endl;
                    TerminateProcess(processHandles[i], 0);
                }
            }
        }
        else if (waitResult == WAIT_FAILED) {
            DWORD error = GetLastError();
            std::cerr << "\nОШИБКА при ожидании завершения процессов!" << std::endl;
            std::cerr << "Код ошибки: " << error << std::endl;
        }
        else {
            std::cout << "\nВсе процессы загрузки успешно завершены." << std::endl;
        }
    }
    else {
        std::cout << "\nНет активных процессов для ожидания." << std::endl;
    }

    // 7. Закрытие дескрипторов и освобождение ресурсов
    std::cout << "\nОсвобождение ресурсов..." << std::endl;

    // Закрываем дескрипторы процессов и потоков
    for (size_t i = 0; i < procInfos.size(); i++) {
        if (procInfos[i].hProcess) {
            CloseHandle(procInfos[i].hProcess);
        }
        if (procInfos[i].hThread) {
            CloseHandle(procInfos[i].hThread);
        }
    }

    // Закрываем дескрипторы объектов синхронизации
    CloseHandle(hSemaphore);
    CloseHandle(hMutex);
    CloseHandle(hEvent);

    std::cout << "Ресурсы освобождены." << std::endl;

    // Выводим статистику выполнения
    printStatistics(M, N, startedCount);

    // Завершение работы программы
    std::cout << "\nБраузер завершил работу." << std::endl;
    std::cout << "Нажмите любую клавишу для выхода...";
    std::cin.get();

    return 0;
}