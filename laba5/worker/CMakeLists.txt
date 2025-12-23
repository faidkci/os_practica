# Создаем исполняемый файл Worker
add_executable(Worker worker.cpp)

# Для Windows добавляем необходимые библиотеки
if(WIN32)
    target_link_libraries(Worker kernel32 user32)
endif()