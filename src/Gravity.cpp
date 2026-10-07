#include "gravitas/Gravity.hpp"

#include <cmath>
#include <cstddef>

namespace gravitas {

void computeAccelerations(std::span<Body> bodies) noexcept {
    for (Body& body : bodies) {
        body.acceleration = Vector3{};
    }

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Vector3 r = bodies[j].position - bodies[i].position; // i -> j
            const double r2 = r.magnitudeSquared();
            if (r2 == 0.0) {
                continue; // Cuerpos coincidentes: ver la política en Gravity.hpp.
            }
            // G r / |r|^3, compartido por ambos cuerpos del par.
            const Vector3 g = r * (kGravitationalConstant / (r2 * std::sqrt(r2)));
            bodies[i].acceleration += g * bodies[j].mass();
            bodies[j].acceleration -= g * bodies[i].mass();
        }
    }
}

double gravitationalPotentialEnergy(std::span<const Body> bodies) noexcept {
    double energy = 0.0;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const double distance = (bodies[j].position - bodies[i].position).magnitude();
            if (distance == 0.0) {
                continue;
            }
            energy -= kGravitationalConstant * bodies[i].mass() * bodies[j].mass() / distance;
        }
    }
    return energy;
}

} // namespace gravitas
