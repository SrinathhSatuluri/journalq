cmake_minimum_required(VERSION 3.20)
project(journalq LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Comma-separated sanitizer list, e.g. "address,undefined" or "thread".
set(JOURNALQ_SANITIZE "" CACHE STRING "Sanitizers to enable")

add_library(journalq
  src/journalq/crc32c.cpp)

target_include_directories(journalq PUBLIC src)
target_compile_options(journalq PRIVATE
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual)

if(JOURNALQ_SANITIZE)
  target_compile_options(journalq PUBLIC
    -fsanitize=${JOURNALQ_SANITIZE} -fno-omit-frame-pointer)
  target_link_options(journalq PUBLIC -fsanitize=${JOURNALQ_SANITIZE})
endif()

enable_testing()
add_subdirectory(tests)
