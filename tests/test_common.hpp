#pragma once

// Arnés de pruebas mínimo y sin dependencias, compartido por los ejecutables de
// prueba de GRAVITAS. Cada ejecutable enumera sus casos de prueba y llama a
// runTests(), cuyo valor de retorno se usa como código de salida para CTest.
// La salida está en español y codificada en UTF-8.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>

#include "Console.hpp"
#include "gravitas/Vector3.hpp"

namespace gravitas::test {

// Tolerancias por defecto para las comparaciones en coma flotante. Dos valores
// son iguales si difieren como mucho en kAbsTolerance, o como mucho en
// kRelTolerance relativa al mayor de los módulos. El término absoluto cubre los
// resultados que deberían ser cero.
inline constexpr double kAbsTolerance = 1e-12;
inline constexpr double kRelTolerance = 1e-12;

[[nodiscard]] inline bool approxEqual(double a, double b,
                                      double absTol = kAbsTolerance,
                                      double relTol = kRelTolerance) {
    if (a == b) {
        return true; // También cubre infinitos iguales.
    }
    const double diff = std::abs(a - b);
    const double scale = std::max(std::abs(a), std::abs(b));
    return diff <= std::max(absTol, relTol * scale);
}

[[nodiscard]] inline bool approxEqual(const Vector3& a, const Vector3& b,
                                      double absTol = kAbsTolerance,
                                      double relTol = kRelTolerance) {
    return approxEqual(a.x, b.x, absTol, relTol)
        && approxEqual(a.y, b.y, absTol, relTol)
        && approxEqual(a.z, b.z, absTol, relTol);
}

// Número de comprobaciones fallidas en el caso de prueba en ejecución.
inline int currentFailures = 0;

inline std::ostream& reportFailure(const char* file, int line) {
    ++currentFailures;
    return std::cerr << "    " << file << ':' << line << ": ";
}

struct TestCase {
    std::string_view name;
    void (*run)();
};

[[nodiscard]] inline int runTests(std::span<const TestCase> tests) {
    const console::Utf8Console utf8Console;
    int failedCases = 0;
    for (const TestCase& test : tests) {
        currentFailures = 0;
        try {
            test.run();
        } catch (const std::exception& e) {
            ++currentFailures;
            std::cerr << "    excepción inesperada: " << e.what() << '\n';
        } catch (...) {
            ++currentFailures;
            std::cerr << "    excepción inesperada no estándar\n";
        }
        const bool passed = currentFailures == 0;
        std::cout << (passed ? "[OK]    " : "[FALLO] ") << test.name << '\n';
        if (!passed) {
            ++failedCases;
        }
    }
    std::cout << '\n' << (tests.size() - static_cast<std::size_t>(failedCases))
              << '/' << tests.size() << " casos de prueba superados\n";
    return failedCases == 0 ? 0 : 1;
}

} // namespace gravitas::test

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            ::gravitas::test::reportFailure(__FILE__, __LINE__)                \
                << "falló CHECK(" #condition ")\n";                            \
        }                                                                      \
    } while (false)

// Funciona con operandos double y Vector3.
#define CHECK_NEAR(actual, expected)                                           \
    do {                                                                       \
        const auto actual_ = (actual);                                         \
        const auto expected_ = (expected);                                     \
        if (!::gravitas::test::approxEqual(actual_, expected_)) {              \
            ::gravitas::test::reportFailure(__FILE__, __LINE__)                \
                << "falló CHECK_NEAR(" #actual ", " #expected "): "            \
                << actual_ << " frente a " << expected_ << '\n';               \
        }                                                                      \
    } while (false)

// Comparación puramente relativa: |actual - esperado| <= relTol * max(|a|, |e|).
// Funciona con operandos double y Vector3 (componente a componente).
#define CHECK_NEAR_REL(actual, expected, relTol)                               \
    do {                                                                       \
        const auto actual_ = (actual);                                         \
        const auto expected_ = (expected);                                     \
        if (!::gravitas::test::approxEqual(actual_, expected_, 0.0, (relTol))) { \
            ::gravitas::test::reportFailure(__FILE__, __LINE__)                \
                << "falló CHECK_NEAR_REL(" #actual ", " #expected ", " #relTol \
                   "): "                                                       \
                << actual_ << " frente a " << expected_ << '\n';               \
        }                                                                      \
    } while (false)

#define CHECK_LE(value, bound)                                                 \
    do {                                                                       \
        const auto value_ = (value);                                           \
        const auto bound_ = (bound);                                           \
        if (!(value_ <= bound_)) {                                             \
            ::gravitas::test::reportFailure(__FILE__, __LINE__)                \
                << "falló CHECK_LE(" #value ", " #bound "): " << value_        \
                << " > " << bound_ << '\n';                                    \
        }                                                                      \
    } while (false)

#define CHECK_THROWS(expression, ExceptionType)                                \
    do {                                                                       \
        bool thrownExpected_ = false;                                          \
        try {                                                                  \
            static_cast<void>(expression);                                     \
        } catch (const ExceptionType&) {                                       \
            thrownExpected_ = true;                                            \
        } catch (...) {                                                        \
        }                                                                      \
        if (!thrownExpected_) {                                                \
            ::gravitas::test::reportFailure(__FILE__, __LINE__)                \
                << "falló CHECK_THROWS(" #expression ", " #ExceptionType ")\n"; \
        }                                                                      \
    } while (false)
