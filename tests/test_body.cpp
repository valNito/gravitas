#include <array>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "gravitas/Body.hpp"
#include "test_common.hpp"

using gravitas::Body;
using gravitas::Vector3;

// --- Garantías en tiempo de compilación --------------------------------------

static_assert(Body::isValidMass(1.0));
static_assert(Body::isValidMass(std::numeric_limits<double>::denorm_min()));
static_assert(Body::isValidMass(std::numeric_limits<double>::max()));
static_assert(!Body::isValidMass(0.0));
static_assert(!Body::isValidMass(-0.0));
static_assert(!Body::isValidMass(-1.0));
static_assert(!Body::isValidMass(std::numeric_limits<double>::infinity()));
static_assert(!Body::isValidMass(-std::numeric_limits<double>::infinity()));
static_assert(!Body::isValidMass(std::numeric_limits<double>::quiet_NaN()));

static_assert(Body{2.0}.mass() == 2.0);

// Un Body no se puede construir por defecto: no existe una masa por defecto con sentido.
static_assert(!std::is_default_constructible_v<Body>);

namespace {

void constructionWithMassOnly() {
    const Body body{5.972e24};
    CHECK_NEAR(body.mass(), 5.972e24);
    CHECK(body.position == Vector3{});
    CHECK(body.velocity == Vector3{});
    CHECK(body.acceleration == Vector3{});
}

void constructionWithFullState() {
    const Vector3 position{3.844e8, 0.0, 0.0};
    const Vector3 velocity{0.0, 1.022e3, 0.0};
    const Body body{7.342e22, position, velocity};

    CHECK_NEAR(body.mass(), 7.342e22);
    CHECK(body.position == position);
    CHECK(body.velocity == velocity);
    CHECK(body.acceleration == Vector3{}); // Siempre empieza en cero.
}

void rejectsInvalidMass() {
    CHECK_THROWS(Body{0.0}, std::invalid_argument);
    CHECK_THROWS(Body{-0.0}, std::invalid_argument);
    CHECK_THROWS(Body{-1.0}, std::invalid_argument);
    CHECK_THROWS(Body{-5.972e24}, std::invalid_argument);
    CHECK_THROWS(Body{std::numeric_limits<double>::quiet_NaN()}, std::invalid_argument);
    CHECK_THROWS(Body{std::numeric_limits<double>::infinity()}, std::invalid_argument);
    CHECK_THROWS(Body{-std::numeric_limits<double>::infinity()}, std::invalid_argument);
}

void acceptsExtremeValidMass() {
    CHECK_NEAR(Body{std::numeric_limits<double>::min()}.mass(),
               std::numeric_limits<double>::min());
    CHECK_NEAR(Body{std::numeric_limits<double>::max()}.mass(),
               std::numeric_limits<double>::max());
}

void kinematicStateIsMutable() {
    Body body{1.0};
    body.position = Vector3{1.0, 2.0, 3.0};
    body.velocity += Vector3{0.5, 0.0, 0.0};
    body.acceleration = Vector3{0.0, 0.0, -9.81};

    CHECK(body.position == (Vector3{1.0, 2.0, 3.0}));
    CHECK(body.velocity == (Vector3{0.5, 0.0, 0.0}));
    CHECK(body.acceleration == (Vector3{0.0, 0.0, -9.81}));
    CHECK_NEAR(body.mass(), 1.0);
}

void copyPreservesState() {
    const Body original{3.0, Vector3{1.0, 1.0, 1.0}, Vector3{2.0, 0.0, 0.0}};
    Body copy = original;
    CHECK_NEAR(copy.mass(), 3.0);
    CHECK(copy.position == original.position);
    CHECK(copy.velocity == original.velocity);

    Body other{10.0};
    other = original;
    CHECK_NEAR(other.mass(), 3.0);
}

} // namespace

int main() {
    constexpr std::array tests{
        gravitas::test::TestCase{"Body: construcción solo con la masa", constructionWithMassOnly},
        gravitas::test::TestCase{"Body: construcción con el estado completo", constructionWithFullState},
        gravitas::test::TestCase{"Body: rechaza masas no válidas", rejectsInvalidMass},
        gravitas::test::TestCase{"Body: acepta masas válidas extremas", acceptsExtremeValidMass},
        gravitas::test::TestCase{"Body: el estado cinemático es modificable", kinematicStateIsMutable},
        gravitas::test::TestCase{"Body: la copia conserva el estado", copyPreservesState},
    };
    return gravitas::test::runTests(tests);
}
