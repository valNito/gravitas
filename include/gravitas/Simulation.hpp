#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gravitas/Body.hpp"

namespace gravitas {

/// Es dueña del estado de una simulación gravitacional de N cuerpos y lo hace
/// avanzar con el integrador Velocity Verlet usando un paso temporal fijo.
///
/// Desde fuera, los cuerpos solo pueden leerse. Así las aceleraciones
/// almacenadas se mantienen coherentes con las posiciones, algo que Velocity
/// Verlet necesita entre pasos.
class Simulation {
public:
    /// @param bodies    Estado inicial (unidades SI).
    /// @param timeStep  Paso de integración en segundos; finito y > 0.
    /// @throws std::invalid_argument si timeStep no es finito y positivo.
    ///
    /// Calcula las aceleraciones gravitatorias iniciales, de modo que bodies()
    /// es totalmente coherente desde el principio.
    Simulation(std::vector<Body> bodies, double timeStep);

    /// Hace avanzar la simulación `count` pasos temporales.
    void step(std::uint64_t count = 1);

    [[nodiscard]] std::span<const Body> bodies() const noexcept { return bodies_; }
    [[nodiscard]] double timeStep() const noexcept { return timeStep_; }
    [[nodiscard]] std::uint64_t stepCount() const noexcept { return stepCount_; }

    /// Tiempo simulado en segundos desde el estado inicial. Se calcula como
    /// stepCount * timeStep, así que no acumula errores de redondeo.
    [[nodiscard]] double time() const noexcept {
        return static_cast<double>(stepCount_) * timeStep_;
    }

private:
    std::vector<Body> bodies_;
    double timeStep_;
    std::uint64_t stepCount_{0};
};

} // namespace gravitas
