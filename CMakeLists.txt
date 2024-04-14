cmake_minimum_required(VERSION 3.10)  # Set the minimum required version of CMake
project(OSLab9 C)                 # Define the project name and the programming language

set(CMAKE_C_STANDARD 17)             # Set the C standard to C99
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add an executable to the project using the specified source file
add_executable(mmap_compute mmap_compute.c)

# Find the pthreads package and link it to your application
find_package(Threads REQUIRED)
target_link_libraries(mmap_compute PRIVATE Threads::Threads)
