cmake_minimum_required(VERSION 3.21)

project(gravitas
    VERSION 0.1.0
    DESCRIPTION "GRAVITAS: motor de simulación gravitacional de N cuerpos"
    LANGUAGES CXX)

# --- Configuración de la compilación -------------------------------------------

# Los generadores de una sola configuración (Ninja, Makefiles) usan Release por
# defecto cuando no se indica el tipo de compilación. Los generadores
# multiconfiguración (Visual Studio) eligen la configuración al compilar con
# --config Debug|Release.
get_property(GRAVITAS_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT GRAVITAS_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilación (Debug o Release)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

# ISO C++20 estricto (-std=c++20 en lugar de -std=gnu++20).
set(CMAKE_CXX_EXTENSIONS OFF)

option(GRAVITAS_BUILD_EXAMPLES "Compilar los programas de ejemplo" ${PROJECT_IS_TOP_LEVEL})
option(GRAVITAS_BUILD_TESTS "Compilar las pruebas automatizadas" ${PROJECT_IS_TOP_LEVEL})
option(GRAVITAS_WARNINGS_AS_ERRORS "Tratar las advertencias del compilador como errores" OFF)

# Aplica el conjunto estricto de advertencias del proyecto a un target que
# compila fuentes. Todo el código fuente está en UTF-8 (comentarios y mensajes
# en español): MSVC lo leería con la página de códigos del sistema sin /utf-8;
# GCC y Clang ya asumen UTF-8.
function(gravitas_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(GRAVITAS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wold-style-cast
            -Wcast-align
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wimplicit-fallthrough
            -Wformat=2)
        if(GRAVITAS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# --- Biblioteca principal ------------------------------------------------------

# Motor físico: tipos matemáticos, gravedad, integrador, estado de la simulación
# y diagnósticos. Independiente de cualquier interfaz de usuario.
add_library(gravitas_core STATIC
    src/Diagnostics.cpp
    src/Gravity.cpp
    src/Scenarios.cpp
    src/Simulation.cpp)
add_library(gravitas::core ALIAS gravitas_core)
target_include_directories(gravitas_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_features(gravitas_core PUBLIC cxx_std_20)
gravitas_enable_warnings(gravitas_core)

# --- Aplicaciones de consola ---------------------------------------------------

# gravitas:           simulación Tierra-Luna.
# gravitas_benchmark: rendimiento de la gravedad directa O(N^2) en cúmulos de Plummer.
add_executable(gravitas app/main.cpp)
add_executable(gravitas_benchmark app/benchmark.cpp)
foreach(app_target IN ITEMS gravitas gravitas_benchmark)
    target_link_libraries(${app_target} PRIVATE gravitas::core)
    gravitas_enable_warnings(${app_target})
endforeach()

# --- Ejemplos ------------------------------------------------------------------

if(GRAVITAS_BUILD_EXAMPLES)
    add_executable(gravitas_example examples/main.cpp)
    target_link_libraries(gravitas_example PRIVATE gravitas::core)
    # Console.hpp: consola en UTF-8 en Windows.
    target_include_directories(gravitas_example PRIVATE "${PROJECT_SOURCE_DIR}/app")
    gravitas_enable_warnings(gravitas_example)
endif()

# --- Pruebas -------------------------------------------------------------------

# Ejecutables de prueba sin dependencias, registrados en CTest. Cada ejecutable
# devuelve un código de salida distinto de cero si falla alguna de sus
# comprobaciones.
if(GRAVITAS_BUILD_TESTS)
    enable_testing()
    foreach(test_name IN ITEMS test_vector3 test_body test_gravity test_simulation
                               test_scenarios)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE gravitas::core)
        # Console.hpp: consola en UTF-8 en Windows.
        target_include_directories(${test_name} PRIVATE "${PROJECT_SOURCE_DIR}/app")
        gravitas_enable_warnings(${test_name})
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
