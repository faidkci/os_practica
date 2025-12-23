#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <cstring>

#pragma pack(push, 1)

// Типы задач
enum class TaskType : uint32_t {
    IMAGE_INVERSION = 1,
    SHUTDOWN = 255  // Команда завершения
};

// Структура задачи
struct Task {
    TaskType type;
    uint32_t task_id;
    uint32_t data_size;          // Количество пикселей
    // Данные будут передаваться после структуры
};

// Структура результата
struct Result {
    uint32_t task_id;
    uint32_t result_size;        // Должно совпадать с data_size
    // Результаты будут передаваться после структуры
};

#pragma pack(pop)

// Константы
constexpr size_t BUFFER_SIZE = 65536;  // 64KB
constexpr int MAX_WORKERS = 10;
constexpr int MAX_TASKS = 1000;

// Имена каналов
inline std::string get_input_pipe_name(int worker_id) {
    return "\\\\.\\pipe\\worker_in_" + std::to_string(worker_id);
}

inline std::string get_output_pipe_name(int worker_id) {
    return "\\\\.\\pipe\\worker_out_" + std::to_string(worker_id);
}

#endif // PROTOCOL_H