cmake_minimum_required(VERSION 3.10)
project(tabu_ecp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Coleta automaticamente todos os .cpp na pasta src/
file(GLOB SOURCES "src/*.cpp")

add_executable(tabu_ecp ${SOURCES})

# Inclui a pasta src/ como local dos headers
target_include_directories(tabu_ecp PRIVATE src)
