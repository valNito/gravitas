#include "gravitas/Simulation.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

#include "gravitas/Gravity.hpp"
#include "gravitas/VelocityVerlet.hpp"

namespace gravitas {

namespace {

double validatedTimeStep(double timeStep) {
    // También rechaza NaN, ya que toda comparación con NaN es falsa.
    if (!(timeStep > 0.0 && timeStep <= std::numeric_limits<double>::max())) {
        throw std::invalid_argument(
            "gravitas::Simulation: el paso temporal debe ser finito y estrictamente positivo (s)");
    }
    return timeStep;
}

} // namespace

Simulation::Simulation(std::vector<Body> bodies, double timeStep)
    : bodies_{std::move(bodies)}, timeStep_{validatedTimeStep(timeStep)} {
    computeAccelerations(bodies_);
}

void Simulation::step(std::uint64_t count) {
    for (std::uint64_t i = 0; i < count; ++i) {
        velocityVerletStep(bodies_, timeStep_, computeAccelerations);
        ++stepCount_;
    }
}

} // namespace gravitas
