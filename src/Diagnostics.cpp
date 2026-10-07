#include "gravitas/Diagnostics.hpp"

#include "gravitas/Gravity.hpp"

namespace gravitas {

double kineticEnergy(std::span<const Body> bodies) noexcept {
    double energy = 0.0;
    for (const Body& body : bodies) {
        energy += 0.5 * body.mass() * body.velocity.magnitudeSquared();
    }
    return energy;
}

double mechanicalEnergy(std::span<const Body> bodies) noexcept {
    return kineticEnergy(bodies) + gravitationalPotentialEnergy(bodies);
}

Vector3 totalMomentum(std::span<const Body> bodies) noexcept {
    Vector3 momentum{};
    for (const Body& body : bodies) {
        momentum += body.velocity * body.mass();
    }
    return momentum;
}

Vector3 angularMomentum(std::span<const Body> bodies) noexcept {
    Vector3 momentum{};
    for (const Body& body : bodies) {
        momentum += body.position.cross(body.velocity) * body.mass();
    }
    return momentum;
}

Vector3 centerOfMass(std::span<const Body> bodies) noexcept {
    Vector3 weighted{};
    double totalMass = 0.0;
    for (const Body& body : bodies) {
        weighted += body.position * body.mass();
        totalMass += body.mass();
    }
    // totalMass > 0 siempre que haya al menos un cuerpo (las masas son positivas).
    return totalMass > 0.0 ? weighted / totalMass : Vector3{};
}

} // namespace gravitas
