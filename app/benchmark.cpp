// Prueba de rendimiento de GRAVITAS: mide el costo de la gravedad actual por
// suma directa (computeAccelerations, O(N^2)) en cúmulos de Plummer de tamaño
// creciente. Es la línea base de rendimiento para optimizaciones futuras como
// Barnes-Hut. La física está en gravitas_core; este archivo solo mide y
// presenta.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Console.hpp"
#include "gravitas/Gravity.hpp"
#include "gravitas/Scenarios.hpp"

namespace {

constexpr std::array<std::size_t, 8> kSizes{100, 200, 500, 1000, 2000, 5000, 10000, 20000};
constexpr std::uint64_t kSeed = 42;
constexpr std::size_t kMinRuns = 3;    // Evaluaciones por tamaño, como mínimo...
constexpr double kMinSeconds = 0.5;    // ...y al menos este tiempo medido.
constexpr std::uint64_t kEarthMoonSteps = 38880; // 27 días a 60 s por paso.

#ifdef NDEBUG
constexpr bool kOptimizedBuild = true;
#else
constexpr bool kOptimizedBuild = false;
#endif

// Se escribe tras cada evaluación para que el compilador no pueda descartar el trabajo.
volatile double g_sink = 0.0;

struct Measurement {
    std::size_t count;
    double seconds; // Mediana del tiempo de una evaluación de fuerzas.
};

// Mediana del tiempo real de una llamada a computeAccelerations(), en segundos.
double measureSeconds(std::vector<gravitas::Body>& bodies) {
    using Clock = std::chrono::steady_clock;
    gravitas::computeAccelerations(bodies); // Calentamiento: cachés y fallos de página.

    std::vector<double> samples;
    double total = 0.0;
    while (samples.size() < kMinRuns || total < kMinSeconds) {
        const auto start = Clock::now();
        gravitas::computeAccelerations(bodies);
        const std::chrono::duration<double> elapsed = Clock::now() - start;
        samples.push_back(elapsed.count());
        total += elapsed.count();
        g_sink = g_sink + bodies.front().acceleration.x;
    }
    std::ranges::sort(samples);
    return samples[samples.size() / 2];
}

// 1234567 -> "1 234 567" (separador de miles al estilo SI, sin ambigüedad en español).
std::string thousands(std::uint64_t value) {
    std::string digits = std::to_string(value);
    for (std::size_t i = digits.size(); i > 3; i -= 3) {
        digits.insert(i - 3, 1, ' ');
    }
    return digits;
}

std::string fixed(double value, int decimals) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}

// Duración con una unidad legible, de milisegundos a días.
std::string duration(double seconds) {
    if (seconds < 1.0) {
        return fixed(seconds * 1e3, 3) + " ms";
    }
    if (seconds < 120.0) {
        return fixed(seconds, 2) + " s";
    }
    if (seconds < 7200.0) {
        return fixed(seconds / 60.0, 1) + " min";
    }
    if (seconds < 172800.0) {
        return fixed(seconds / 3600.0, 1) + " h";
    }
    return fixed(seconds / 86400.0, 1) + " días";
}

std::uint64_t pairCount(std::size_t count) {
    const auto n = static_cast<std::uint64_t>(count);
    return n * (n - 1) / 2;
}

} // namespace

int main() {
    using namespace gravitas;
    const console::Utf8Console utf8Console;

    std::cout << "GRAVITAS — Rendimiento del cálculo gravitacional directo\n\n"
              << "Método:      suma directa por pares, O(N²) (computeAccelerations)\n"
              << "Escenario:   esfera de Plummer, 1 masa solar por estrella, radio de escala "
                 "1 pc, semilla " << kSeed << '\n'
              << "Medición:    mediana de al menos " << kMinRuns << " evaluaciones y "
              << kMinSeconds << " s por tamaño\n"
              << "Compilación: "
              << (kOptimizedBuild ? "optimizada (Release)"
                                  : "Debug — los tiempos NO son representativos; usa Release")
              << '\n'
              << "Hilos:       1 (el cálculo no está paralelizado; este equipo tiene "
              << std::thread::hardware_concurrency() << ")\n\n";

    std::cout << std::setw(8) << "N" << std::setw(16) << "Pares" << std::setw(16) << "Tiempo/eval."
              << std::setw(11) << "ns/par" << std::setw(12) << "Mpares/s" << std::setw(12)
              << "Exponente" << '\n'
              << std::string(75, '-') << '\n';

    std::vector<Measurement> results;
    for (const std::size_t count : kSizes) {
        std::vector<Body> bodies = scenarios::makePlummerSphere(
            count, static_cast<double>(count) * scenarios::kSolarMass, scenarios::kParsec, kSeed);
        const double seconds = measureSeconds(bodies);
        const auto pairs = static_cast<double>(pairCount(count));

        std::string exponent = "-";
        if (!results.empty()) {
            const Measurement& previous = results.back();
            exponent = fixed(std::log(seconds / previous.seconds)
                                 / std::log(static_cast<double>(count)
                                            / static_cast<double>(previous.count)),
                             2);
        }
        results.push_back({count, seconds});

        std::cout << std::setw(8) << thousands(count) << std::setw(16)
                  << thousands(pairCount(count)) << std::setw(16) << duration(seconds)
                  << std::setw(11) << fixed(seconds / pairs * 1e9, 2) << std::setw(12)
                  << fixed(pairs / seconds / 1e6, 1) << std::setw(12) << exponent << '\n';
    }

    // Se extrapola desde el tamaño mayor, donde domina el término O(N^2).
    const Measurement& largest = results.back();
    auto projected = [&largest](double count) {
        const double ratio = count / static_cast<double>(largest.count);
        return largest.seconds * ratio * ratio;
    };

    std::cout << "\nExponente: pendiente log-log del tiempo entre filas consecutivas; "
                 "O(N²) implica 2.\n"
              << "Velocity Verlet hace una evaluación por paso, así que Tiempo/eval. es "
                 "también el costo de un paso.\n\n"
              << "Proyección O(N²) desde N = " << thousands(largest.count) << ":\n"
              << "  N =   100 000  ->  " << duration(projected(1e5)) << " por evaluación\n"
              << "  N = 1 000 000  ->  " << duration(projected(1e6)) << " por evaluación\n"
              << "  27 días con Δt = 60 s (" << thousands(kEarthMoonSteps)
              << " pasos) y N = 100 000  ->  "
              << duration(projected(1e5) * static_cast<double>(kEarthMoonSteps)) << '\n';
    return 0;
}
