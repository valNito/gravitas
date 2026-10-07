// Aplicación de consola de GRAVITAS: ejecuta la simulación idealizada de dos
// cuerpos Tierra-Luna e imprime la evolución de las posiciones junto con
// diagnósticos de conservación. Este archivo solo se ocupa de la presentación;
// la física está en gravitas_core. El texto para el usuario está en español y
// codificado en UTF-8.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "Console.hpp"
#include "gravitas/Diagnostics.hpp"
#include "gravitas/Scenarios.hpp"
#include "gravitas/Simulation.hpp"

namespace {

constexpr double kTimeStep = 60.0;          // s
constexpr int kDurationDays = 27;
constexpr double kSecondsPerDay = 86400.0;
constexpr double kMetresPerKm = 1000.0;

// Número entero de pasos por día, comprobado en tiempo de compilación.
constexpr std::uint64_t kStepsPerDay = static_cast<std::uint64_t>(kSecondsPerDay / kTimeStep);
static_assert(static_cast<double>(kStepsPerDay) * kTimeStep == kSecondsPerDay);

// Ancho visible de las etiquetas del resumen.
constexpr std::size_t kLabelWidth = 31;

// Imprime "  <texto>" rellenado hasta kLabelWidth columnas visibles. Cuenta
// puntos de código UTF-8 en lugar de bytes, así que las etiquetas con tildes
// se alinean igual que las ASCII.
void printLabel(std::string_view text) {
    const auto columns = static_cast<std::size_t>(std::count_if(
        text.begin(), text.end(),
        [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
    std::cout << "  " << text << std::string(kLabelWidth - std::min(columns, kLabelWidth), ' ');
}

void printRow(int day, std::span<const gravitas::Body> bodies) {
    const gravitas::Body& earth = bodies[0];
    const gravitas::Body& moon = bodies[1];
    std::cout << std::setw(3) << day
              << std::setw(15) << earth.position.x / kMetresPerKm
              << std::setw(15) << earth.position.y / kMetresPerKm
              << std::setw(15) << moon.position.x / kMetresPerKm
              << std::setw(15) << moon.position.y / kMetresPerKm << '\n';
}

double separation(std::span<const gravitas::Body> bodies) {
    return (bodies[1].position - bodies[0].position).magnitude();
}

// Suma de |m v| sobre todos los cuerpos: la escala natural con la que se
// compara el momento total (idealmente nulo).
double momentumScale(std::span<const gravitas::Body> bodies) {
    double scale = 0.0;
    for (const gravitas::Body& body : bodies) {
        scale += body.mass() * body.velocity.magnitude();
    }
    return scale;
}

void printMomentum(std::string_view label, const gravitas::Vector3& momentum, double scale) {
    printLabel(label);
    std::cout << momentum << " kg·m/s\n"
              << std::string(kLabelWidth + 2, ' ') << "|P| = " << momentum.magnitude()
              << " kg·m/s = " << momentum.magnitude() / scale << " × Σ|m·v|\n";
}

} // namespace

int main() {
    using namespace gravitas;

    [[maybe_unused]] const console::Utf8Console utf8Console;

    Simulation simulation{scenarios::makeEarthMoon(), kTimeStep};

    const double initialDistance = separation(simulation.bodies());
    const double initialEnergy = mechanicalEnergy(simulation.bodies());
    const Vector3 initialMomentum = totalMomentum(simulation.bodies());
    const double initialMomentumScale = momentumScale(simulation.bodies());

    std::cout << "GRAVITAS — Simulación de N cuerpos\n\n"
              << "Escenario: Tierra-Luna\n"
              << "Cuerpos: " << simulation.bodies().size() << '\n'
              << "Integrador: Velocity Verlet\n"
              << "Paso temporal: " << simulation.timeStep() << " segundos\n"
              << "Duración: " << kDurationDays << " días\n\n"
              << "Posiciones en km, relativas al centro de masas.\n\n";

    std::cout << "Día" << std::setw(15) << "Tierra X" << std::setw(15) << "Tierra Y"
              << std::setw(15) << "Luna X" << std::setw(15) << "Luna Y" << '\n'
              << std::string(63, '-') << '\n';
    std::cout << std::fixed << std::setprecision(3);

    printRow(0, simulation.bodies());
    for (int day = 1; day <= kDurationDays; ++day) {
        simulation.step(kStepsPerDay);
        printRow(day, simulation.bodies());
    }

    const double finalDistance = separation(simulation.bodies());
    const double finalEnergy = mechanicalEnergy(simulation.bodies());

    std::cout << "\nResumen\n";
    printLabel("Pasos:");
    std::cout << simulation.stepCount() << '\n';
    printLabel("Tiempo simulado:");
    std::cout << simulation.time() / kSecondsPerDay << " días\n";
    printLabel("Distancia inicial:");
    std::cout << initialDistance / kMetresPerKm << " km\n";
    printLabel("Distancia final:");
    std::cout << finalDistance / kMetresPerKm << " km\n";
    printLabel("Cambio de distancia:");
    std::cout << finalDistance - initialDistance << " m\n";

    std::cout << std::scientific << std::setprecision(9);
    printLabel("Energía mecánica inicial:");
    std::cout << initialEnergy << " J\n";
    printLabel("Energía mecánica final:");
    std::cout << finalEnergy << " J\n";
    std::cout << std::setprecision(3);
    printLabel("Error relativo de energía:");
    std::cout << std::abs((finalEnergy - initialEnergy) / initialEnergy) << '\n';

    printMomentum("Momento lineal total inicial:", initialMomentum, initialMomentumScale);
    printMomentum("Momento lineal total final:", totalMomentum(simulation.bodies()),
                  momentumScale(simulation.bodies()));

    return 0;
}
