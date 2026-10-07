#include "gravitas/Scenarios.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>

#include "gravitas/Gravity.hpp"

namespace gravitas::scenarios {

namespace {

// Números double uniformes en [0, 1) construidos a partir de la salida cruda de
// 64 bits del Mersenne Twister, que el estándar de C++ especifica por completo
// (std::uniform_real_distribution no), así que una semilla produce la misma
// secuencia con cualquier biblioteca estándar.
class UniformRandom {
public:
    explicit UniformRandom(std::uint64_t seed) : engine_{seed} {}

    double next() { return static_cast<double>(engine_() >> 11) * 0x1.0p-53; }

private:
    std::mt19937_64 engine_;
};

// Vector unitario distribuido uniformemente sobre la esfera.
Vector3 randomDirection(UniformRandom& random) {
    const double z = 2.0 * random.next() - 1.0;
    const double phi = 2.0 * std::numbers::pi * random.next();
    const double s = std::sqrt(1.0 - z * z);
    return {s * std::cos(phi), s * std::sin(phi), z};
}

void validateBinary(double primaryMass, double secondaryMass, double separation) {
    if (!Body::isValidMass(primaryMass) || !Body::isValidMass(secondaryMass)) {
        throw std::invalid_argument(
            "gravitas::scenarios: las masas deben ser finitas y estrictamente positivas (kg)");
    }
    if (!(separation > 0.0 && separation <= std::numeric_limits<double>::max())) {
        throw std::invalid_argument(
            "gravitas::scenarios: la separación debe ser finita y estrictamente positiva (m)");
    }
}

} // namespace

std::vector<Body> makeCircularBinary(double primaryMass, double secondaryMass,
                                     double separation) {
    validateBinary(primaryMass, secondaryMass, separation);

    const double totalMass = primaryMass + secondaryMass;
    const double primaryRadius = separation * secondaryMass / totalMass;
    const double secondaryRadius = separation * primaryMass / totalMass;
    const double angularVelocity =
        std::sqrt(kGravitationalConstant * totalMass / (separation * separation * separation));

    return {
        Body{primaryMass,
             Vector3{-primaryRadius, 0.0, 0.0},
             Vector3{0.0, -angularVelocity * primaryRadius, 0.0}},
        Body{secondaryMass,
             Vector3{secondaryRadius, 0.0, 0.0},
             Vector3{0.0, angularVelocity * secondaryRadius, 0.0}},
    };
}

double circularBinaryPeriod(double primaryMass, double secondaryMass, double separation) {
    validateBinary(primaryMass, secondaryMass, separation);
    const double totalMass = primaryMass + secondaryMass;
    return 2.0 * std::numbers::pi
         * std::sqrt(separation * separation * separation
                     / (kGravitationalConstant * totalMass));
}

std::vector<Body> makeEarthMoon() {
    return makeCircularBinary(kEarthMass, kMoonMass, kEarthMoonDistance);
}

std::vector<Body> makePlummerSphere(std::size_t count, double totalMass, double scaleRadius,
                                    std::uint64_t seed) {
    if (count == 0) {
        throw std::invalid_argument(
            "gravitas::scenarios: una esfera de Plummer necesita al menos un cuerpo");
    }
    if (!Body::isValidMass(totalMass) || !Body::isValidMass(totalMass / static_cast<double>(count))) {
        throw std::invalid_argument(
            "gravitas::scenarios: la masa de cada cuerpo debe ser finita y estrictamente "
            "positiva (kg)");
    }
    if (!(scaleRadius > 0.0 && scaleRadius <= std::numeric_limits<double>::max())) {
        throw std::invalid_argument(
            "gravitas::scenarios: el radio de escala debe ser finito y estrictamente positivo (m)");
    }

    const double bodyMass = totalMass / static_cast<double>(count);
    const double velocityScale = std::sqrt(kGravitationalConstant * totalMass / scaleRadius);
    UniformRandom random{seed};

    std::vector<Body> bodies;
    bodies.reserve(count);
    Vector3 positionSum{};
    Vector3 velocitySum{};

    for (std::size_t i = 0; i < count; ++i) {
        // Radio: se invierte la fracción de masa acumulada m = r^3 / (r^2 + a^2)^(3/2).
        // m está en [0, 1), así que el radio siempre es finito (m = 0 da r = 0).
        const double massFraction = random.next();
        const double radius =
            scaleRadius / std::sqrt(std::pow(massFraction, -2.0 / 3.0) - 1.0);

        // Rapidez: una fracción q de la velocidad de escape local, distribuida
        // como g(q) ~ q^2 (1 - q^2)^(7/2). Muestreo de rechazo bajo la cota 0,1,
        // que supera el máximo de g (unos 0,092, en q^2 = 2/9).
        double q = 0.0;
        do {
            q = random.next();
        } while (0.1 * random.next() > q * q * std::pow(1.0 - q * q, 3.5));
        const double ratio = radius / scaleRadius;
        const double escapeSpeed =
            std::sqrt(2.0) * velocityScale * std::pow(1.0 + ratio * ratio, -0.25);

        const Vector3 position = randomDirection(random) * radius;
        const Vector3 velocity = randomDirection(random) * (q * escapeSpeed);
        bodies.emplace_back(bodyMass, position, velocity);
        positionSum += position;
        velocitySum += velocity;
    }

    // Masas iguales: el centro de masas y su velocidad son medias simples.
    const Vector3 centreOfMass = positionSum / static_cast<double>(count);
    const Vector3 centreOfMassVelocity = velocitySum / static_cast<double>(count);
    for (Body& body : bodies) {
        body.position -= centreOfMass;
        body.velocity -= centreOfMassVelocity;
    }
    return bodies;
}

} // namespace gravitas::scenarios
