#pragma once

#include <span>

#include "gravitas/Body.hpp"

namespace gravitas {

/// Hace avanzar cada cuerpo un paso de Velocity Verlet de duración dt
/// (segundos):
///
///     x_{n+1} = x_n + v_n dt + 1/2 a_n dt^2
///     v_{n+1} = v_n + 1/2 (a_n + a_{n+1}) dt
///
/// El integrador no sabe qué fuerza actúa sobre los cuerpos: después de
/// moverlos llama a computeAccelerations(bodies), que debe sobrescribir la
/// aceleración de cada cuerpo con a(x_{n+1}) (p. ej.,
/// gravitas::computeAccelerations).
///
/// Precondición: la aceleración de cada cuerpo ya contiene a_n = a(x_n). La
/// a_{n+1} calculada al final de un paso es exactamente la a_n del siguiente,
/// así que la fuerza se evalúa una vez por paso en lugar de dos.
/// Postcondición: las posiciones, velocidades y aceleraciones describen el
/// paso n+1.
///
/// dt no se valida aquí (ruta crítica); Simulation lo comprueba una vez.
template <typename ComputeAccelerations>
void velocityVerletStep(std::span<Body> bodies, double dt,
                        ComputeAccelerations&& computeAccelerations) {
    const double halfDt = 0.5 * dt;
    const double halfDtSquared = 0.5 * dt * dt;

    for (Body& body : bodies) {
        body.position += body.velocity * dt + body.acceleration * halfDtSquared;
        body.velocity += body.acceleration * halfDt; // 1/2 a_n dt
    }

    computeAccelerations(bodies); // a_{n+1}

    for (Body& body : bodies) {
        body.velocity += body.acceleration * halfDt; // 1/2 a_{n+1} dt
    }
}

} // namespace gravitas
