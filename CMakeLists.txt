cmake_minimum_required(VERSION 3.10)
project(tabu_ecp LANGUAGES CXX)

# Define C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- CONFIGURAÇÃO DE BUILD (OTIMIZAÇÃO) ---
# Se o usuário não especificou o tipo de build, força "Release"
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Nenhum tipo de build selecionado, forçando RELEASE (Otimizado).")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Adiciona flags de otimização específicas para GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # -O2: Otimização robusta (segura e rápida)
    # -march=native: Usa instruções específicas do seu processador (AVX, etc)
    add_compile_options(-O2 -march=native -Wall)
elseif(MSVC)
    # Flags para Visual Studio (/O2 é o padrão de Release, mas garantimos aqui)
    add_compile_options(/O2)
endif()

# Coleta arquivos fonte
file(GLOB SOURCES "src/*.cpp")

# Cria executável
add_executable(tabu_ecp ${SOURCES})

# Includes
target_include_directories(tabu_ecp PRIVATE src)