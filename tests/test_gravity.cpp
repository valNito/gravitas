#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "gravitas/Gravity.hpp"
#include "gravitas/Scenarios.hpp"
#include "test_common.hpp"

using gravitas::Body;
using gravitas::computeAccelerations;
using gravitas::kGravitationalConstant;
using gravitas::Vector3;

// Justificación de la tolerancia: una aceleración entre un par es el resultado
// de menos de diez operaciones en coma flotante, cada una con un error relativo
// de redondeo de como mucho 2^-53 ~ 1,1e-16. 1e-13 deja un margen de unas 100
// veces sobre esa cota y aun así detecta cualquier error de fórmula (potencia
// de r equivocada, masa equivocada, falta de G).
constexpr double kRoundingTolerance = 1e-13;

namespace {

void accelerationMagnitudeSimple() {
    // Dos cuerpos de 1e10 kg a 1 m: a = G m / r^2 = 6.67430e-11 * 1e10 = 0.667430.
    std::vector<Body> bodies{Body{1e10, Vector3{0.0, 0.0, 0.0}},
                             Body{1e10, Vector3{1.0, 0.0, 0.0}}};
    computeAccelerations(bodies);
    CHECK_NEAR_REL(bodies[0].acceleration.x, 0.667430, kRoundingTolerance);
    CHECK_NEAR_REL(bodies[1].acceleration.x, -0.667430, kRoundingTolerance);
    CHECK(bodies[0].acceleration.y == 0.0 && bodies[0].acceleration.z == 0.0);
}

void accelerationMagnitudeEarthMoon() {
    using namespace gravitas::scenarios;
    const double d = kEarthMoonDistance;
    std::vector<Body> bodies{Body{kEarthMass, Vector3{0.0, 0.0, 0.0}},
                             Body{kMoonMass, Vector3{d, 0.0, 0.0}}};
    computeAccelerations(bodies);

    const double expectedEarth = kGravitationalConstant * kMoonMass / (d * d);
    const double expectedMoon = kGravitationalConstant * kEarthMass / (d * d);
    CHECK_NEAR_REL(bodies[0].acceleration.magnitude(), expectedEarth, kRoundingTolerance);
    CHECK_NEAR_REL(bodies[1].acceleration.magnitude(), expectedMoon, kRoundingTolerance);
    // Comprobación de cordura frente al valor conocido: ~2,70e-3 m/s^2 en la Luna.
    CHECK(bodies[1].acceleration.magnitude() > 2.69e-3);
    CHECK(bodies[1].acceleration.magnitude() < 2.71e-3);
}

void attractionDirection() {
    // Separación (3, 4, 12) m, |r| = 13 m, deliberadamente no alineada con los ejes.
    const Vector3 p1{1.0, 2.0, 3.0};
    const Vector3 p2{4.0, 6.0, 15.0};
    const double m1 = 1e12;
    const double m2 = 3e12;
    std::vector<Body> bodies{Body{m1, p1}, Body{m2, p2}};
    computeAccelerations(bodies);

    const Vector3 r = p2 - p1;
    const Vector3 expected1 = r * (kGravitationalConstant * m2 / (13.0 * 13.0 * 13.0));
    const Vector3 expected2 = -r * (kGravitationalConstant * m1 / (13.0 * 13.0 * 13.0));

    CHECK_LE((bodies[0].acceleration - expected1).magnitude(),
             kRoundingTolerance * expected1.magnitude());
    CHECK_LE((bodies[1].acceleration - expected2).magnitude(),
             kRoundingTolerance * expected2.magnitude());

    // Cada cuerpo es atraído hacia el otro: paralelo a la separación...
    CHECK_NEAR(bodies[0].acceleration.normalized(), r.normalized());
    CHECK_NEAR(bodies[1].acceleration.normalized(), -r.normalized());
    // ...y con el sentido atractivo (no repulsivo).
    CHECK(bodies[0].acceleration.dot(r) > 0.0);
    CHECK(bodies[1].acceleration.dot(r) < 0.0);
}

void newtonsThirdLaw() {
    std::vector<Body> bodies{
        Body{5.972e24, Vector3{0.0, 0.0, 0.0}},
        Body{7.342e22, Vector3{3.844e8, 1.0e7, -2.0e6}},
        Body{1.0e20, Vector3{-1.5e8, 2.5e8, 4.0e7}},
        Body{3.3e23, Vector3{9.0e7, -6.0e8, 1.2e8}},
    };
    computeAccelerations(bodies);

    // Las fuerzas se cancelan por pares, así que la suma total m_i a_i debe anularse.
    Vector3 totalForce{};
    double largestForce = 0.0;
    for (const Body& body : bodies) {
        const Vector3 force = body.acceleration * body.mass();
        totalForce += force;
        largestForce = std::max(largestForce, force.magnitude());
    }
    CHECK(largestForce > 0.0);
    CHECK_LE(totalForce.magnitude(), kRoundingTolerance * largestForce);

    // Caso de dos cuerpos: m1 a1 = -m2 a2.
    std::vector<Body> pair{bodies[0], bodies[1]};
    computeAccelerations(pair);
    const Vector3 f1 = pair[0].acceleration * pair[0].mass();
    const Vector3 f2 = pair[1].acceleration * pair[1].mass();
    CHECK_LE((f1 + f2).magnitude(), kRoundingTolerance * f1.magnitude());
}

void coincidentBodiesAreSkipped() {
    // Política documentada: un par a distancia cero no aporta fuerza.
    std::vector<Body> pair{Body{1e20, Vector3{5.0, 5.0, 5.0}},
                           Body{2e20, Vector3{5.0, 5.0, 5.0}}};
    computeAccelerations(pair);
    CHECK(pair[0].acceleration == Vector3{});
    CHECK(pair[1].acceleration == Vector3{});

    // Con un tercer cuerpo, el par coincidente sigue sintiendo al tercero y el
    // tercero siente a ambos; nada se vuelve inf ni NaN.
    std::vector<Body> trio{Body{1e20, Vector3{}}, Body{2e20, Vector3{}},
                           Body{3e20, Vector3{1000.0, 0.0, 0.0}}};
    computeAccelerations(trio);
    for (const Body& body : trio) {
        CHECK(std::isfinite(body.acceleration.x) && std::isfinite(body.acceleration.y)
              && std::isfinite(body.acceleration.z));
    }
    CHECK(trio[0].acceleration == trio[1].acceleration);
    CHECK_NEAR_REL(trio[0].acceleration.x, kGravitationalConstant * 3e20 / 1e6,
                   kRoundingTolerance);
    CHECK_NEAR_REL(trio[2].acceleration.x, -kGravitationalConstant * 3e20 / 1e6,
                   kRoundingTolerance);
}

void accelerationsAreOverwritten() {
    std::vector<Body> single{Body{1e24, Vector3{1.0, 2.0, 3.0}}};
    single[0].acceleration = Vector3{9.0, 9.0, 9.0};
    computeAccelerations(single);
    CHECK(single[0].acceleration == Vector3{}); // Sin autointeracción.

    std::vector<Body> pair{Body{1e20, Vector3{}}, Body{1e20, Vector3{100.0, 0.0, 0.0}}};
    computeAccelerations(pair);
    const Vector3 first = pair[0].acceleration;
    computeAccelerations(pair); // No debe acumular.
    CHECK(pair[0].acceleration == first);

    std::vector<Body> empty;
    computeAccelerations(empty); // No debe fallar.
}

void potentialEnergy() {
    const double d = 3.844e8;
    std::vector<Body> pair{Body{5.972e24, Vector3{}}, Body{7.342e22, Vector3{0.0, d, 0.0}}};
    CHECK_NEAR_REL(gravitas::gravitationalPotentialEnergy(pair),
                   -kGravitationalConstant * 5.972e24 * 7.342e22 / d, kRoundingTolerance);

    // Tres cuerpos en los vértices de un triángulo rectángulo 3-4-5.
    std::vector<Body> trio{Body{1e10, Vector3{0.0, 0.0, 0.0}},
                           Body{2e10, Vector3{3.0, 0.0, 0.0}},
                           Body{3e10, Vector3{0.0, 4.0, 0.0}}};
    const double expected = -kGravitationalConstant
                          * (1e10 * 2e10 / 3.0 + 1e10 * 3e10 / 4.0 + 2e10 * 3e10 / 5.0);
    CHECK_NEAR_REL(gravitas::gravitationalPotentialEnergy(trio), expected, kRoundingTolerance);

    // El par coincidente se omite, en coherencia con la fuerza.
    std::vector<Body> coincident{Body{1e10, Vector3{}}, Body{1e10, Vector3{}}};
    CHECK(gravitas::gravitationalPotentialEnergy(coincident) == 0.0);
}

} // namespace

int main() {
    constexpr std::array tests{
        gravitas::test::TestCase{"Gravedad: módulo de la aceleración (caso simple)", accelerationMagnitudeSimple},
        gravitas::test::TestCase{"Gravedad: módulo de la aceleración (Tierra-Luna)", accelerationMagnitudeEarthMoon},
        gravitas::test::TestCase{"Gravedad: dirección de la atracción", attractionDirection},
        gravitas::test::TestCase{"Gravedad: tercera ley de Newton", newtonsThirdLaw},
        gravitas::test::TestCase{"Gravedad: se omiten los cuerpos coincidentes", coincidentBodiesAreSkipped},
        gravitas::test::TestCase{"Gravedad: las aceleraciones se sobrescriben", accelerationsAreOverwritten},
        gravitas::test::TestCase{"Gravedad: energía potencial", potentialEnergy},
    };
    return gravitas::test::runTests(tests);
}
