cmake_minimum_required(VERSION 3.16)
project(QtPyDynamicUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Python3 REQUIRED COMPONENTS Development)
find_package(pybind11 REQUIRED CONFIG)

add_executable(qt_py main.cpp)

target_link_libraries(qt_py PRIVATE
    Qt6::Widgets
    pybind11::embed
    Python3::Python
)

target_include_directories(qt_py PRIVATE
    ${Python3_INCLUDE_DIRS}
)

file(COPY ${CMAKE_SOURCE_DIR}/tasks DESTINATION ${CMAKE_BINARY_DIR})
