cmake_minimum_required(VERSION 3.15)
project(USD_Device)

# Устанавливаем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Для Windows
if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# Создаем библиотеку из файлов в папке src
add_library(usd_device
    src/usd_device.cpp
)

# Указываем папку с заголовочными файлами
target_include_directories(usd_device
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Создаем исполняемый файл с примером
add_executable(usd_example
    examples/example_usage.cpp
)

# Линкуем библиотеку и системные зависимости
target_link_libraries(usd_example
    usd_device
    ws2_32
)

# Устанавливаем usd_example как главный исполняемый файл
set_target_properties(usd_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)