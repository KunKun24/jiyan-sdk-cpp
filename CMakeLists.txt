cmake_minimum_required(VERSION 3.16)
project(jiyan_sdk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JNI is required by SDK_Init.cpp
find_package(JNI REQUIRED)

add_library(jiyan_sdk STATIC include/jiyan/SDK_Main.cpp)
target_include_directories(jiyan_sdk PUBLIC include/jiyan ${JNI_INCLUDE_DIRS})

option(JIYAN_BUILD_TESTS "Build crypto vector tests" ON)
if(JIYAN_BUILD_TESTS)
    add_executable(crypto_vectors tests/crypto_vectors.cpp)
    target_include_directories(crypto_vectors PRIVATE include/jiyan)
endif()
