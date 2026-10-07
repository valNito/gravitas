#include <array>
#include <cmath>
#include <vector>

#include "gravitas/Diagnostics.hpp"
#include "gravitas/Gravity.hpp"
#include "gravitas/Scenarios.hpp"
#include "test_common.hpp"

using gravitas::Body;
using gravitas::Vector3;
namespace scenarios = gravitas::scenarios;

namespace {

// Salvo donde se indica, los operandos son enteros pequeños: todos los
// productos y sumas son exactos en double, así que se compara con ==.

void angularMomentumOfNothing() {
    const std::vector<Body> empty;
    CHECK(gravitas::angularMomentum(empty) == Vector3{});
}

void angularMomentumOfOneBody() {
    // r x v = (1, 2, 3) x (4, 5, 6) = (-3, 6, -3); por la masa 2.
    const std::vector<Body> bodies{Body{2.0, Vector3{1.0, 2.0, 3.0}, Vector3{4.0, 5.0, 6.0}}};
    CHECK(gravitas::angularMomentum(bodies) == (Vector3{-6.0, 12.0, -6.0}));

    // Movimiento radial (v paralela a r): no hay momento angular.
    const std::vector<Body> radial{Body{3.0, Vector3{1.0, 2.0, 3.0}, Vector3{2.0, 4.0, 6.0}}};
    CHECK(gravitas::angularMomentum(radial) == Vector3{});
}

void angularMomentumIsAdditive() {
    // (1, 0, 0) x (0, 1, 0) = (0, 0, 1), por la masa 3;
    // (0, 2, 0) x (0, 0, 5) = (10, 0, 0), por la masa 1.
    const std::vector<Body> bodies{Body{3.0, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}},
                                   Body{1.0, Vector3{0.0, 2.0, 0.0}, Vector3{0.0, 0.0, 5.0}}};
    CHECK(gravitas::angularMomentum(bodies) == (Vector3{10.0, 0.0, 3.0}));
}

void angularMomentumReferencePoint() {
    // Respecto de un punto c, L_c = suma m (r - c) x v = L - c x P. Mover el
    // origen a c equivale a restar c a todas las posiciones.
    const Vector3 c{2.0, -1.0, 3.0};
    const std::vector<Body> bodies{Body{1.0, Vector3{1.0, 2.0, 0.0}, Vector3{0.0, 1.0, 2.0}},
                                   Body{2.0, Vector3{-3.0, 0.0, 1.0}, Vector3{1.0, 0.0, -1.0}},
                                   Body{3.0, Vector3{0.0, -2.0, 4.0}, Vector3{2.0, 1.0, 0.0}}};
    std::vector<Body> shifted = bodies;
    for (Body& body : shifted) {
        body.position -= c;
    }
    const Vector3 momentum = gravitas::totalMomentum(bodies);
    CHECK(momentum != Vector3{}); // Con P != 0, L depende del punto de referencia.
    CHECK(gravitas::angularMomentum(shifted)
          == gravitas::angularMomentum(bodies) - c.cross(momentum));

    // Con P = 0 (binaria circular en su centro de masas), L no depende del
    // punto. Valores reales: se admite el redondeo de términos de tamaño
    // m |r| |v| con |r| ~ 1e9 m, del orden de 1e-15 |L|.
    const std::vector<Body> binary = scenarios::makeEarthMoon();
    std::vector<Body> farAway = binary;
    for (Body& body : farAway) {
        body.position += Vector3{1.0e9, -2.0e9, 5.0e8};
    }
    const Vector3 l = gravitas::angularMomentum(binary);
    CHECK_LE((gravitas::angularMomentum(farAway) - l).magnitude(), 1e-13 * l.magnitude());
}

void angularMomentumOfCircularBinary() {
    // Solución analítica: dos cuerpos que giran con velocidad angular w
    // alrededor de su centro de masas tienen L = mu d^2 w, con la masa
    // reducida mu = m1 m2 / (m1 + m2), perpendicular al plano de la órbita
    // (+z, porque giran en sentido antihorario).
    const double m1 = scenarios::kEarthMass;
    const double m2 = scenarios::kMoonMass;
    const double d = scenarios::kEarthMoonDistance;
    const double w = std::sqrt(gravitas::kGravitationalConstant * (m1 + m2) / (d * d * d));
    const double expected = m1 * m2 / (m1 + m2) * d * d * w;

    const Vector3 l = gravitas::angularMomentum(scenarios::makeEarthMoon());
    CHECK(l.x == 0.0 && l.y == 0.0); // Órbita en el plano xy.
    CHECK_NEAR_REL(l.z, expected, 1e-14);
}

} // namespace

int main() {
    constexpr std::array tests{
        gravitas::test::TestCase{"Momento angular: sin cuerpos", angularMomentumOfNothing},
        gravitas::test::TestCase{"Momento angular: un cuerpo", angularMomentumOfOneBody},
        gravitas::test::TestCase{"Momento angular: es aditivo", angularMomentumIsAdditive},
        gravitas::test::TestCase{"Momento angular: dependencia del punto de referencia", angularMomentumReferencePoint},
        gravitas::test::TestCase{"Momento angular: binaria circular frente a la solución analítica", angularMomentumOfCircularBinary},
    };
    return gravitas::test::runTests(tests);
}
