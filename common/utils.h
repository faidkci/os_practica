#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <thread>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
// Для Unix/Linux реализация
#endif

// Генерация случайных данных для задачи
inline std::vector<uint8_t> generate_random_pixels(size_t count) {
    std::vector<uint8_t> pixels(count);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < count; ++i) {
        pixels[i] = static_cast<uint8_t>(dis(gen));
    }

    return pixels;
}

// Вычисление инверсии изображения (вариант 14)
inline std::vector<uint8_t> invert_image(const std::vector<uint8_t>& pixels) {
    std::vector<uint8_t> result(pixels.size());

    for (size_t i = 0; i < pixels.size(); ++i) {
        result[i] = 255 - pixels[i];
    }

    return result;
}

// Имитация "тяжелой" работы
inline void simulate_heavy_work() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Проверка существования файла
inline bool file_exists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributes(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    // Для Linux
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
#endif
}

// Получение директории исполняемого файла
inline std::string get_exe_directory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    return path.substr(0, pos);
#else
    // Для Linux
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string path(buffer);
        size_t pos = path.find_last_of("/");
        return path.substr(0, pos);
    }
    return ".";
#endif
}

#endif // UTILS_H