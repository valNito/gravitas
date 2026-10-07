#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "gravitas/Diagnostics.hpp"
#include "gravitas/Gravity.hpp"
#include "gravitas/Scenarios.hpp"
#include "test_common.hpp"

using gravitas::Body;
using gravitas::Vector3;
namespace scenarios = gravitas::scenarios;

namespace {

constexpr std::size_t kCount = 2000;
constexpr double kTotalMass = 2000.0 * scenarios::kSolarMass;
constexpr double kScaleRadius = scenarios::kParsec;
constexpr std::uint64_t kSeed = 42;

std::vector<Body> makeCluster(std::uint64_t seed = kSeed) {
    return scenarios::makePlummerSphere(kCount, kTotalMass, kScaleRadius, seed);
}

void report(const char* what, double measured, double expected, double tolerance) {
    std::cout << "    " << what << ": " << measured << " (esperado " << expected
              << ", tolerancia " << tolerance << ")\n";
}

void rejectsInvalidParameters() {
    CHECK_THROWS(scenarios::makePlummerSphere(0, kTotalMass, kScaleRadius, kSeed),
                 std::invalid_argument);
    CHECK_THROWS(scenarios::makePlummerSphere(10, -1.0, kScaleRadius, kSeed),
                 std::invalid_argument);
    CHECK_THROWS(scenarios::makePlummerSphere(10, kTotalMass, 0.0, kSeed),
                 std::invalid_argument);
}

void isDeterministic() {
    const std::vector<Body> a = makeCluster();
    const std::vector<Body> b = makeCluster();
    const std::vector<Body> c = makeCluster(kSeed + 1);
    bool identical = true;
    bool different = false;
    for (std::size_t i = 0; i < kCount; ++i) {
        identical = identical && a[i].position == b[i].position && a[i].velocity == b[i].velocity;
        different = different || a[i].position != c[i].position;
    }
    CHECK(identical);  // Misma semilla: cuerpos idénticos bit a bit.
    CHECK(different);  // Otra semilla: otro cúmulo.
}

void massesAndCentreOfMass() {
    const std::vector<Body> bodies = makeCluster();
    CHECK(bodies.size() == kCount);

    double totalMass = 0.0;
    double maxRadius = 0.0;
    double momentumScale = 0.0;
    for (const Body& body : bodies) {
        CHECK(body.mass() == bodies[0].mass());  // Masas iguales.
        totalMass += body.mass();
        maxRadius = std::max(maxRadius, body.position.magnitude());
        momentumScale += body.mass() * body.velocity.magnitude();
    }
    CHECK_NEAR_REL(totalMass, kTotalMass, 1e-12);

    // Centro de masas en reposo en el origen, salvo el redondeo que deja restar
    // la media (relativo al tamaño del cúmulo y de los momentos implicados).
    CHECK_LE(gravitas::centerOfMass(bodies).magnitude(), 1e-12 * maxRadius);
    CHECK_LE(gravitas::totalMomentum(bodies).magnitude(), 1e-12 * momentumScale);
}

// Las comprobaciones restantes comparan el cúmulo muestreado con el modelo
// analítico de Plummer. Una muestra de N cuerpos se desvía del modelo en
// O(1/sqrt(N)), alrededor de un 2,2 % para N = 2000; las tolerancias de abajo
// son de 2 a 4 veces ese valor.

void halfMassRadius() {
    std::vector<double> radii;
    for (const Body& body : makeCluster()) {
        radii.push_back(body.position.magnitude());
    }
    std::ranges::nth_element(radii, radii.begin() + kCount / 2);
    const double measured = radii[kCount / 2] / kScaleRadius;
    const double expected = 1.0 / std::sqrt(std::pow(2.0, 2.0 / 3.0) - 1.0); // ~1,305
    report("radio de media masa / a", measured, expected, 0.05 * expected);
    CHECK_NEAR_REL(measured, expected, 0.05);
}

void virialEquilibrium() {
    const std::vector<Body> bodies = makeCluster();
    const double kinetic = gravitas::kineticEnergy(bodies);
    const double potential = gravitas::gravitationalPotentialEnergy(bodies);
    const double virialRatio = 2.0 * kinetic / -potential;
    report("cociente virial 2K / |U|", virialRatio, 1.0, 0.1);
    CHECK(std::abs(virialRatio - 1.0) <= 0.1);
}

void totalEnergy() {
    const std::vector<Body> bodies = makeCluster();
    const double measured = gravitas::mechanicalEnergy(bodies);
    const double expected = -(3.0 * std::numbers::pi / 64.0) * gravitas::kGravitationalConstant
                          * kTotalMass * kTotalMass / kScaleRadius;
    report("E / E_analítica", measured / expected, 1.0, 0.1);
    CHECK_NEAR_REL(measured, expected, 0.1);
}

// Mayor desviación del tensor de direcciones <n_i n_j> respecto del valor
// isótropo delta_ij / 3, sobre todas las componentes.
double directionTensorDeviation(const std::vector<Vector3>& vectors) {
    std::array<std::array<double, 3>, 3> tensor{};
    for (const Vector3& v : vectors) {
        const Vector3 n = v.normalized();
        const std::array<double, 3> c{n.x, n.y, n.z};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                tensor[i][j] += c[i] * c[j];
            }
        }
    }
    double deviation = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            const double expected = i == j ? 1.0 / 3.0 : 0.0;
            deviation = std::max(
                deviation, std::abs(tensor[i][j] / static_cast<double>(vectors.size()) - expected));
        }
    }
    return deviation;
}

void isotropy() {
    // Las direcciones isótropas cumplen <n_i n_j> = delta_ij / 3. Cada
    // componente tiene una dispersión estadística de unos 0,3 / sqrt(N) ~ 0,007
    // para N = 2000; la tolerancia 0,03 equivale a ~4 sigma, mientras que, por
    // ejemplo, direcciones agrupadas en los polos (ángulo polar uniforme) darían
    // <n_z^2> = 1/2, desviado en 0,17.
    //
    // El vector unitario medio no se usa para las posiciones: la cola no
    // truncada de Plummer (estrellas más allá de 100 a) desplaza el centro de
    // masas ~0,1 a respecto del centro de densidad, lo que sesga las direcciones
    // de las estrellas del núcleo medidas desde él.
    const std::vector<Body> bodies = makeCluster();
    std::vector<Vector3> positions;
    std::vector<Vector3> velocities;
    Vector3 meanVelocityDirection{};
    for (const Body& body : bodies) {
        positions.push_back(body.position);
        velocities.push_back(body.velocity);
        meanVelocityDirection += body.velocity.normalized();
    }
    meanVelocityDirection /= static_cast<double>(kCount);

    const double positionDeviation = directionTensorDeviation(positions);
    const double velocityDeviation = directionTensorDeviation(velocities);
    report("max |<n_i n_j> - delta_ij/3|, posiciones", positionDeviation, 0.0, 0.03);
    report("max |<n_i n_j> - delta_ij/3|, velocidades", velocityDeviation, 0.0, 0.03);
    CHECK_LE(positionDeviation, 0.03);
    CHECK_LE(velocityDeviation, 0.03);

    // Las velocidades están acotadas por la velocidad de escape, así que su
    // dirección media es una comprobación limpia: módulo esperado ~ 1/sqrt(N) ~ 0,022.
    report("|dirección media de la velocidad|", meanVelocityDirection.magnitude(), 0.0, 0.07);
    CHECK_LE(meanVelocityDirection.magnitude(), 0.07);
}

} // namespace

int main() {
    constexpr std::array tests{
        gravitas::test::TestCase{"Plummer: rechaza parámetros no válidos", rejectsInvalidParameters},
        gravitas::test::TestCase{"Plummer: es determinista para una semilla", isDeterministic},
        gravitas::test::TestCase{"Plummer: masas y centro de masas", massesAndCentreOfMass},
        gravitas::test::TestCase{"Plummer: radio de media masa", halfMassRadius},
        gravitas::test::TestCase{"Plummer: equilibrio virial", virialEquilibrium},
        gravitas::test::TestCase{"Plummer: energía total", totalEnergy},
        gravitas::test::TestCase{"Plummer: isotropía", isotropy},
    };
    return gravitas::test::runTests(tests);
}
