cmake_minimum_required(VERSION 3.15)
project(USD_Device)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

add_library(usd_device
    src/usd_device.cpp
)


target_include_directories(usd_device
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


add_executable(usd_example
    examples/example_usage.cpp
)


target_link_libraries(usd_example
    usd_device
    ws2_32
)


set_target_properties(usd_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)