# Создаем исполняемый файл Browser
add_executable(Browser browser.cpp)

# Для Windows добавляем необходимые библиотеки
if(WIN32)
    target_link_libraries(Browser kernel32 user32)
endif()