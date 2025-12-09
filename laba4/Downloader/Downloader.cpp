#include <windows.h>
#include <iostream>
#include <string>
#include <random>
#include <algorithm>
#include <cmath>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cctype>

// =================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===================

// Безопасная версия isspace для char
bool safe_isspace(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

// 1. Обратить порядок байт в массиве
void reverseBytes(unsigned char* arr, size_t size) {
    for (size_t i = 0; i < size / 2; ++i) {
        std::swap(arr[i], arr[size - 1 - i]);
    }
}

// 2. Посчитать количество слов в строке (используем безопасную версию)
int countWords(const std::string& str) {
    int count = 0;
    bool inWord = false;
    for (char c : str) {
        if (safe_isspace(c)) {
            inWord = false;
        }
        else if (!inWord) {
            inWord = true;
            ++count;
        }
    }
    return count;
}

// 3. Отсортировать массив символов
void sortCharArray(char* arr, size_t size) {
    std::sort(arr, arr + size);
}

// 4. Найти стандартное отклонение
double standardDeviation(double* arr, size_t size) {
    if (size == 0) return 0.0;

    double mean = 0.0;
    for (size_t i = 0; i < size; ++i) {
        mean += arr[i];
    }
    mean /= size;

    double sum = 0.0;
    for (size_t i = 0; i < size; ++i) {
        double diff = arr[i] - mean;
        sum += diff * diff;
    }

    return std::sqrt(sum / size);
}

// 5. Посчитать скобки
std::pair<int, int> countBrackets(const std::string& str) {
    int open = 0, close = 0;
    for (char c : str) {
        if (c == '(') open++;
        else if (c == ')') close++;
    }
    return { open, close };
}

// 6. Найти самую длинную строку
std::string longestString(const std::vector<std::string>& strs) {
    if (strs.empty()) return "";

    std::string longest = strs[0];
    for (const auto& s : strs) {
        if (s.length() > longest.length()) {
            longest = s;
        }
    }
    return longest;
}

// 7. Проверить сбалансированность скобок
bool isBalanced(const std::string& str) {
    std::vector<char> stack;

    for (char c : str) {
        if (c == '(' || c == '[' || c == '{') {
            stack.push_back(c);
        }
        else if (c == ')') {
            if (stack.empty() || stack.back() != '(') return false;
            stack.pop_back();
        }
        else if (c == ']') {
            if (stack.empty() || stack.back() != '[') return false;
            stack.pop_back();
        }
        else if (c == '}') {
            if (stack.empty() || stack.back() != '{') return false;
            stack.pop_back();
        }
    }

    return stack.empty();
}

// 8. Найти произведение элементов матрицы
long long matrixProduct(int matrix[10][10]) {
    long long product = 1;
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            product *= matrix[i][j];
        }
    }
    return product;
}

// 9. Посчитать простые числа (решето Эратосфена)
int countPrimes(int n) {
    if (n < 2) return 0;

    std::vector<bool> isPrime(n + 1, true);
    isPrime[0] = isPrime[1] = false;

    for (int i = 2; i * i <= n; ++i) {
        if (isPrime[i]) {
            for (int j = i * i; j <= n; j += i) {
                isPrime[j] = false;
            }
        }
    }

    int count = 0;
    for (int i = 2; i <= n; ++i) {
        if (isPrime[i]) count++;
    }

    return count;
}

// =================== ОСНОВНАЯ ФУНКЦИЯ ===================

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "rus");
    if (argc < 2) {
        std::cerr << "Использование: Downloader.exe <вариант>" << std::endl;
        return 1;
    }

    int variant = std::stoi(argv[1]);
    DWORD pid = GetCurrentProcessId();

    // 1. Открытие объектов синхронизации
    HANDLE hSemaphore = OpenSemaphoreA(
        SYNCHRONIZE | SEMAPHORE_MODIFY_STATE,
        FALSE,
        "DownloadSlots"
    );

    HANDLE hMutex = OpenMutexA(
        SYNCHRONIZE | MUTEX_MODIFY_STATE,
        FALSE,
        "LogAccessMutex"
    );

    HANDLE hEvent = OpenEventA(
        SYNCHRONIZE,
        FALSE,
        "BrowserClosingEvent"
    );

    if (!hSemaphore || !hMutex || !hEvent) {
        DWORD error = GetLastError();
        std::cerr << "[PID: " << pid << "] Ошибка открытия объектов синхронизации! Код: "
            << error << std::endl;
        return 1;
    }

    // 2. Ожидание слота или события закрытия
    WaitForSingleObject(hMutex, INFINITE);
    std::cout << "[PID: " << std::setw(6) << pid
        << "] Ожидание свободного слота для загрузки файла" << variant << ".ext..." << std::endl;
    ReleaseMutex(hMutex);

    HANDLE waitHandles[] = { hSemaphore, hEvent };

    // Ждем либо семафор (слот), либо событие закрытия
    DWORD waitResult = WaitForMultipleObjects(
        2,
        waitHandles,
        FALSE,
        INFINITE
    );

    // Проверяем, что произошло
    if (waitResult == WAIT_OBJECT_0 + 1) {
        // Сработало событие закрытия браузера
        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << std::setw(6) << pid
            << "] ✗ Загрузка прервана (браузер закрывается)" << std::endl;
        ReleaseMutex(hMutex);

        CloseHandle(hSemaphore);
        CloseHandle(hMutex);
        CloseHandle(hEvent);
        return 0;
    }

    // 3. Начало загрузки (получили слот)
    WaitForSingleObject(hMutex, INFINITE);
    std::cout << "[PID: " << std::setw(6) << pid
        << "] ✓ Слот получен. Начало загрузки 'file" << variant << ".ext'..." << std::endl;
    ReleaseMutex(hMutex);

    // 4. Имитация загрузки и обработки файла
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sleepDist(1000, 3000);

    // Имитируем время загрузки
    Sleep(sleepDist(gen));

    // Выполняем индивидуальное задание
    bool taskCompleted = false;
    std::string taskDescription;

    try {
        switch (variant) {
        case 1: {
            taskDescription = "обработка массива байт (2048 элементов)";
            unsigned char arr[2048];
            for (int i = 0; i < 2048; ++i) {
                arr[i] = static_cast<unsigned char>(i % 256);
            }
            reverseBytes(arr, 2048);
            taskCompleted = true;
            break;
        }

        case 2: {
            taskDescription = "подсчет слов в тексте (используем английский текст)";
            std::string text = "Any browser limits the number of simultaneous downloads "
                "(for example, no more than 6) so as not to overload the network connection. "
                "Other downloads are queued.";
            int words = countWords(text);
            taskCompleted = (words > 0);
            break;
        }

        case 3: {
            taskDescription = "сортировка массива символов (500 элементов)";
            char arr[500];
            for (int i = 0; i < 500; ++i) {
                arr[i] = static_cast<char>(gen() % 128); // Используем только ASCII
            }
            sortCharArray(arr, 500);
            taskCompleted = true;
            break;
        }

        case 4: {
            taskDescription = "расчет стандартного отклонения (200 чисел)";
            double arr[200];
            for (int i = 0; i < 200; ++i) {
                arr[i] = static_cast<double>(gen() % 100);
            }
            double dev = standardDeviation(arr, 200);
            taskCompleted = true;
            break;
        }

        case 5: {
            taskDescription = "подсчет скобок в выражении";
            std::string expr = "(a + b) * (c - d) / (e + f) - (g * h)";
            auto brackets = countBrackets(expr);
            taskCompleted = true;
            break;
        }

        case 6: {
            taskDescription = "поиск самой длинной строки";
            std::vector<std::string> strs = {
                "Short string",
                "Medium length string",
                "Very long string with many characters for testing",
                "Another string",
                "The longest string in this dataset for testing purposes"
            };
            std::string longest = longestString(strs);
            taskCompleted = true;
            break;
        }

        case 7: {
            taskDescription = "проверка баланса скобок";
            std::string expr1 = "{[()]}";
            std::string expr2 = "{[(])}";
            bool balanced1 = isBalanced(expr1);
            bool balanced2 = isBalanced(expr2);
            taskCompleted = true;
            break;
        }

        case 8: {
            taskDescription = "произведение элементов матрицы 10x10";
            int matrix[10][10];
            for (int i = 0; i < 10; ++i) {
                for (int j = 0; j < 10; ++j) {
                    matrix[i][j] = (gen() % 10) + 1;
                }
            }
            long long prod = matrixProduct(matrix);
            taskCompleted = true;
            break;
        }

        case 9: {
            taskDescription = "подсчет простых чисел до 10000";
            int primes = countPrimes(10000);
            taskCompleted = true;
            break;
        }

        default: {
            taskDescription = "неизвестная операция";
            taskCompleted = false;
            break;
        }
        }
    }
    catch (const std::exception& e) {
        WaitForSingleObject(hMutex, INFINITE);
        std::cerr << "[PID: " << std::setw(6) << pid
            << "] ✗ Ошибка обработки файла: " << e.what() << std::endl;
        ReleaseMutex(hMutex);
        taskCompleted = false;
    }

    // 5. Завершение работы
    if (taskCompleted) {
        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << std::setw(6) << pid
            << "] ✓ Файл 'file" << variant << ".ext' успешно обработан" << std::endl;
        std::cout << "[PID: " << std::setw(6) << pid
            << "]   Операция: " << taskDescription << std::endl;
        ReleaseMutex(hMutex);

        // Освобождаем слот
        ReleaseSemaphore(hSemaphore, 1, NULL);

        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << std::setw(6) << pid
            << "]   Слот освобожден" << std::endl;
        ReleaseMutex(hMutex);
    }
    else {
        WaitForSingleObject(hMutex, INFINITE);
        std::cout << "[PID: " << std::setw(6) << pid
            << "] ✗ Ошибка обработки файла 'file" << variant << ".ext'" << std::endl;
        ReleaseMutex(hMutex);
    }

    // 6. Закрытие дескрипторов
    CloseHandle(hSemaphore);
    CloseHandle(hMutex);
    CloseHandle(hEvent);

    return taskCompleted ? 0 : 1;
}