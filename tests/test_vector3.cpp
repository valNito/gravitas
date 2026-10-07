#include <array>
#include <cmath>
#include <type_traits>

#include "gravitas/Vector3.hpp"
#include "test_common.hpp"

using gravitas::Vector3;

// --- Garantías en tiempo de compilación --------------------------------------

static_assert(std::is_trivially_copyable_v<Vector3>);
static_assert(std::is_aggregate_v<Vector3>);
static_assert(sizeof(Vector3) == 3 * sizeof(double));

// La aritmética y los productos escalar y vectorial se pueden usar en
// expresiones constantes.
static_assert(Vector3{1.0, 2.0, 3.0} + Vector3{1.0, 1.0, 1.0} == Vector3{2.0, 3.0, 4.0});
static_assert(Vector3{1.0, 0.0, 0.0}.cross(Vector3{0.0, 1.0, 0.0}) == Vector3{0.0, 0.0, 1.0});
static_assert(Vector3{1.0, 2.0, 3.0}.dot(Vector3{4.0, 5.0, 6.0}) == 32.0);

namespace {

// Operandos elegidos para que los resultados esperados sean exactamente representables.
constexpr Vector3 kA{1.0, 2.0, 3.0};
constexpr Vector3 kB{4.0, -5.0, 6.0};

void construction() {
    const Vector3 zero{};
    CHECK(zero.x == 0.0 && zero.y == 0.0 && zero.z == 0.0);

    const Vector3 v{1.5, -2.5, 3.5};
    CHECK(v.x == 1.5 && v.y == -2.5 && v.z == 3.5);

    const Vector3 partial{.y = 7.0};
    CHECK(partial == (Vector3{0.0, 7.0, 0.0}));
}

void addition() {
    CHECK_NEAR(kA + kB, (Vector3{5.0, -3.0, 9.0}));
    CHECK_NEAR(kA + kB, kB + kA);
    CHECK_NEAR(kA + Vector3{}, kA);

    Vector3 v = kA;
    v += kB;
    CHECK_NEAR(v, (Vector3{5.0, -3.0, 9.0}));
}

void subtraction() {
    CHECK_NEAR(kA - kB, (Vector3{-3.0, 7.0, -3.0}));
    CHECK_NEAR(kA - kA, Vector3{});
    CHECK_NEAR(-kA, (Vector3{-1.0, -2.0, -3.0}));
    CHECK_NEAR(kA - kB, kA + (-kB));

    Vector3 v = kA;
    v -= kB;
    CHECK_NEAR(v, (Vector3{-3.0, 7.0, -3.0}));
}

void scalarMultiplication() {
    CHECK_NEAR(kA * 2.0, (Vector3{2.0, 4.0, 6.0}));
    CHECK_NEAR(2.0 * kA, kA * 2.0);
    CHECK_NEAR(kA * -1.0, -kA);
    CHECK_NEAR(kA * 0.0, Vector3{});
    CHECK_NEAR(kA * 0.1, (Vector3{0.1, 0.2, 0.3}));

    Vector3 v = kA;
    v *= 3.0;
    CHECK_NEAR(v, (Vector3{3.0, 6.0, 9.0}));
}

void scalarDivision() {
    CHECK_NEAR((Vector3{2.0, 4.0, 6.0}) / 2.0, kA);
    CHECK_NEAR(kA / 3.0, (Vector3{1.0 / 3.0, 2.0 / 3.0, 1.0}));
    CHECK_NEAR((kA * 7.0) / 7.0, kA);

    Vector3 v{3.0, 6.0, 9.0};
    v /= 3.0;
    CHECK_NEAR(v, kA);
}

void dotProduct() {
    CHECK_NEAR(kA.dot(kB), 12.0); // 4 - 10 + 18
    CHECK_NEAR(kA.dot(kB), kB.dot(kA));
    CHECK_NEAR((Vector3{1.0, 0.0, 0.0}).dot(Vector3{0.0, 1.0, 0.0}), 0.0);
    CHECK_NEAR(kA.dot(kA), 14.0);
    CHECK_NEAR(kA.dot(Vector3{}), 0.0);
}

void crossProduct() {
    constexpr Vector3 ex{1.0, 0.0, 0.0};
    constexpr Vector3 ey{0.0, 1.0, 0.0};
    constexpr Vector3 ez{0.0, 0.0, 1.0};

    // Base dextrógira.
    CHECK_NEAR(ex.cross(ey), ez);
    CHECK_NEAR(ey.cross(ez), ex);
    CHECK_NEAR(ez.cross(ex), ey);

    const Vector3 a{1.0, 2.0, 3.0};
    const Vector3 b{4.0, 5.0, 6.0};
    CHECK_NEAR(a.cross(b), (Vector3{-3.0, 6.0, -3.0}));
    CHECK_NEAR(a.cross(b), -b.cross(a));  // Anticonmutativo.
    CHECK_NEAR(a.cross(a), Vector3{});    // Vectores paralelos.
    CHECK_NEAR(a.cross(b).dot(a), 0.0);   // Ortogonal a ambos operandos.
    CHECK_NEAR(a.cross(b).dot(b), 0.0);
}

void magnitudes() {
    const Vector3 v{3.0, 4.0, 12.0};
    CHECK_NEAR(v.magnitudeSquared(), 169.0);
    CHECK_NEAR(v.magnitude(), 13.0);
    CHECK_NEAR((-v).magnitude(), 13.0);
    CHECK_NEAR(Vector3{}.magnitude(), 0.0);
    CHECK_NEAR(Vector3{}.magnitudeSquared(), 0.0);

    // Una escala astronómica (unos 10,6 años luz) queda holgadamente dentro del rango.
    // (No se llama "far": <windows.h>, incluido por Console.hpp, define esa macro.)
    const Vector3 distant{1e17, 0.0, 0.0};
    CHECK_NEAR(distant.magnitude(), 1e17);
    CHECK_NEAR(distant.magnitudeSquared(), 1e34);
}

void normalization() {
    const Vector3 v{3.0, 4.0, 12.0};
    const Vector3 unit = v.normalized();
    CHECK_NEAR(unit, (Vector3{3.0 / 13.0, 4.0 / 13.0, 12.0 / 13.0}));
    CHECK_NEAR(unit.magnitude(), 1.0);
    CHECK_NEAR(unit.cross(v), Vector3{}); // Misma dirección...
    CHECK(unit.dot(v) > 0.0);             // ...y mismo sentido.
    CHECK(v == (Vector3{3.0, 4.0, 12.0}));  // El original no cambia.

    CHECK_NEAR((Vector3{0.0, -5.0, 0.0}).normalized(), (Vector3{0.0, -1.0, 0.0}));
    CHECK_NEAR((Vector3{1e17, 1e17, 0.0}).normalized().magnitude(), 1.0);
    CHECK_NEAR((Vector3{1e-100, 0.0, 0.0}).normalized(), (Vector3{1.0, 0.0, 0.0}));
}

void nullVectorNormalization() {
    // Política documentada: un vector nulo se normaliza al vector cero.
    const Vector3 n = Vector3{}.normalized();
    CHECK(n == Vector3{});
    CHECK(!std::isnan(n.x) && !std::isnan(n.y) && !std::isnan(n.z));

    CHECK((Vector3{-0.0, 0.0, -0.0}).normalized() == Vector3{});

    // El módulo al cuadrado se anula por subdesbordamiento: se trata como vector nulo.
    CHECK((Vector3{1e-200, 0.0, 0.0}).normalized() == Vector3{});
}

} // namespace

int main() {
    constexpr std::array tests{
        gravitas::test::TestCase{"Vector3: construcción", construction},
        gravitas::test::TestCase{"Vector3: suma", addition},
        gravitas::test::TestCase{"Vector3: resta", subtraction},
        gravitas::test::TestCase{"Vector3: multiplicación por escalar", scalarMultiplication},
        gravitas::test::TestCase{"Vector3: división por escalar", scalarDivision},
        gravitas::test::TestCase{"Vector3: producto escalar", dotProduct},
        gravitas::test::TestCase{"Vector3: producto vectorial", crossProduct},
        gravitas::test::TestCase{"Vector3: módulos", magnitudes},
        gravitas::test::TestCase{"Vector3: normalización", normalization},
        gravitas::test::TestCase{"Vector3: normalización del vector nulo", nullVectorNormalization},
    };
    return gravitas::test::runTests(tests);
}
