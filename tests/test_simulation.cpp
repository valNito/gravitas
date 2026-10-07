#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "gravitas/Diagnostics.hpp"
#include "gravitas/Gravity.hpp"
#include "gravitas/Scenarios.hpp"
#include "gravitas/Simulation.hpp"
#include "gravitas/VelocityVerlet.hpp"
#include "test_common.hpp"

using gravitas::Body;
using gravitas::Simulation;
using gravitas::Vector3;
namespace scenarios = gravitas::scenarios;

namespace {

constexpr double kSecondsPerDay = 86400.0;

// Imprime una métrica medida junto a su tolerancia, para que el registro de las
// pruebas documente cuánto margen tiene realmente cada comprobación física.
void report(const char* what, double measured, double tolerance) {
    std::cout << "    " << what << ": " << measured << " (tolerancia " << tolerance << ")\n";
}

double momentumScale(std::span<const Body> bodies) {
    double scale = 0.0;
    for (const Body& body : bodies) {
        scale += body.mass() * body.velocity.magnitude();
    }
    return scale;
}

double separation(std::span<const Body> bodies) {
    return (bodies[1].position - bodies[0].position).magnitude();
}

// Sistema Tierra-Luna que parte del apoápside con una fracción de la velocidad
// circular: una órbita excéntrica (speedFactor 0,7 da e ~ 0,51), que exige al
// integrador mucho más que una circular. Escalar ambas velocidades mantiene el
// momento total en cero.
std::vector<Body> makeEccentricEarthMoon(double speedFactor) {
    std::vector<Body> bodies = scenarios::makeEarthMoon();
    for (Body& body : bodies) {
        body.velocity *= speedFactor;
    }
    return bodies;
}

// Tres cuerpos en 3D con una deriva neta (momento total != 0): la Tierra, la
// Luna y un tercer cuerpo fuera del plano, todos con una velocidad común
// añadida. Sirve para comprobar la conservación y no solo que una cantidad "se
// mantiene cerca de cero".
std::vector<Body> makeDriftingThreeBody() {
    std::vector<Body> bodies = scenarios::makeEarthMoon();
    bodies.emplace_back(1.0e22, Vector3{0.0, 2.0e8, 1.0e7}, Vector3{500.0, 0.0, 100.0});
    for (Body& body : bodies) {
        body.velocity += Vector3{100.0, -50.0, 20.0};
    }
    return bodies;
}

// Suma de m |r x v| sobre todos los cuerpos: la escala natural con la que se
// compara la variación del momento angular total. A diferencia de |L|, no se
// reduce cuando las contribuciones de los cuerpos se cancelan entre sí.
double angularMomentumScale(std::span<const Body> bodies) {
    double scale = 0.0;
    for (const Body& body : bodies) {
        scale += body.mass() * body.position.cross(body.velocity).magnitude();
    }
    return scale;
}

// Máxima variación relativa del momento angular total a lo largo de `steps`
// pasos de Velocity Verlet, medida después de cada paso.
double maxRelativeAngularMomentumDrift(std::vector<Body> bodies, double dt,
                                       std::uint64_t steps) {
    Simulation simulation{std::move(bodies), dt};
    const Vector3 l0 = gravitas::angularMomentum(simulation.bodies());
    const double scale = angularMomentumScale(simulation.bodies());
    double maxDrift = 0.0;
    for (std::uint64_t i = 0; i < steps; ++i) {
        simulation.step();
        const Vector3 drift = gravitas::angularMomentum(simulation.bodies()) - l0;
        maxDrift = std::max(maxDrift, drift.magnitude() / scale);
    }
    return maxDrift;
}

// --- Gestión del estado de la simulación ---------------------------------------

void rejectsInvalidTimeStep() {
    CHECK_THROWS(Simulation(scenarios::makeEarthMoon(), 0.0), std::invalid_argument);
    CHECK_THROWS(Simulation(scenarios::makeEarthMoon(), -60.0), std::invalid_argument);
    CHECK_THROWS(Simulation(scenarios::makeEarthMoon(),
                            std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
    CHECK_THROWS(Simulation(scenarios::makeEarthMoon(),
                            std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
}

void initialStateIsConsistent() {
    const Simulation simulation{scenarios::makeEarthMoon(), 60.0};
    CHECK(simulation.stepCount() == 0);
    CHECK(simulation.time() == 0.0);
    CHECK(simulation.timeStep() == 60.0);

    // El constructor calcula a_0, como exige Velocity Verlet.
    std::vector<Body> expected = scenarios::makeEarthMoon();
    gravitas::computeAccelerations(expected);
    CHECK(simulation.bodies()[0].acceleration == expected[0].acceleration);
    CHECK(simulation.bodies()[1].acceleration == expected[1].acceleration);
    CHECK(simulation.bodies()[1].acceleration.magnitude() > 0.0);
}

void stepAdvancesTime() {
    Simulation simulation{scenarios::makeEarthMoon(), 60.0};
    const Vector3 moonStart = simulation.bodies()[1].position;
    simulation.step();
    simulation.step(9);
    CHECK(simulation.stepCount() == 10);
    CHECK(simulation.time() == 600.0);
    CHECK(simulation.bodies()[1].position != moonStart);
}

void verletStepMatchesFormulas() {
    // Un paso con aceleración constante, caso en que Velocity Verlet es exacto:
    //   x1 = x0 + v0 dt + 1/2 a dt^2,  v1 = v0 + a dt.
    std::vector<Body> bodies{Body{1.0, Vector3{1.0, 2.0, 3.0}, Vector3{4.0, 0.0, -1.0}}};
    const Vector3 a{0.0, -9.81, 0.5};
    bodies[0].acceleration = a;
    const double dt = 2.0;
    gravitas::velocityVerletStep(bodies, dt, [&a](std::span<Body> b) {
        for (Body& body : b) {
            body.acceleration = a;
        }
    });
    CHECK_NEAR(bodies[0].position, (Vector3{1.0 + 8.0, 2.0 - 19.62, 3.0 - 2.0 + 1.0}));
    CHECK_NEAR(bodies[0].velocity, (Vector3{4.0, -19.62, -1.0 + 1.0}));
}

// --- Escenario de binaria circular ------------------------------------------------

void circularBinaryInitialConditions() {
    const std::vector<Body> bodies = scenarios::makeEarthMoon();
    const double d = scenarios::kEarthMoonDistance;

    CHECK_NEAR_REL(separation(bodies), d, 1e-15);
    // Centro de masas en el origen y momento total nulo, salvo el redondeo de
    // cantidades del tamaño de d y de suma|m v|, respectivamente.
    CHECK_LE(gravitas::centerOfMass(bodies).magnitude(), 1e-15 * d);
    CHECK_LE(gravitas::totalMomentum(bodies).magnitude(), 1e-15 * momentumScale(bodies));

    // Movimiento circular: la aceleración gravitatoria proporciona exactamente
    // la aceleración centrípeta v^2 / r respecto del centro de masas y es
    // perpendicular a la velocidad.
    std::vector<Body> withAcceleration = bodies;
    gravitas::computeAccelerations(withAcceleration);
    for (const Body& body : withAcceleration) {
        const double r = body.position.magnitude();
        CHECK_NEAR_REL(body.acceleration.magnitude(), body.velocity.magnitudeSquared() / r, 1e-13);
        CHECK_LE(std::abs(body.acceleration.normalized().dot(body.velocity.normalized())), 1e-15);
    }

    CHECK_THROWS(scenarios::makeCircularBinary(1.0, 1.0, 0.0), std::invalid_argument);
    CHECK_THROWS(scenarios::makeCircularBinary(-1.0, 1.0, 1.0), std::invalid_argument);
}

// --- Validación física del movimiento integrado ---------------------------------

void momentumConservation() {
    const std::vector<Body> bodies = makeDriftingThreeBody();
    const Vector3 initial = gravitas::totalMomentum(bodies);
    const double scale = momentumScale(bodies);

    Simulation simulation{bodies, 60.0};
    double maxDrift = 0.0;
    for (int day = 0; day < 14; ++day) {
        simulation.step(1440);
        const Vector3 drift = gravitas::totalMomentum(simulation.bodies()) - initial;
        maxDrift = std::max(maxDrift, drift.magnitude() / scale);
    }
    // Las fuerzas se cancelan por pares en cada paso, así que el momento solo
    // cambia por redondeo (~1e-16 por operación). 1e-12 admite su acumulación a
    // lo largo de 20160 pasos y aun así pondría en evidencia cualquier fuerza
    // asimétrica.
    report("max |P - P0| / suma|m v|", maxDrift, 1e-12);
    CHECK_LE(maxDrift, 1e-12);
}

void circularOrbitStability() {
    // Se integra exactamente un periodo orbital, con dt = T / 40000 (~59 s), y
    // se compara cada paso con la solución analítica exacta de la órbita
    // circular: cada cuerpo gira alrededor del origen a w = 2 pi / T.
    const double period = scenarios::circularBinaryPeriod(
        scenarios::kEarthMass, scenarios::kMoonMass, scenarios::kEarthMoonDistance);
    constexpr std::uint64_t kSteps = 40000;
    const double dt = period / static_cast<double>(kSteps);
    const double w = 2.0 * std::numbers::pi / period;
    const double d = scenarios::kEarthMoonDistance;

    const std::vector<Body> initial = scenarios::makeEarthMoon();
    Simulation simulation{initial, dt};

    double maxRadiusError = 0.0;
    double maxPositionError = 0.0;
    for (std::uint64_t i = 1; i <= kSteps; ++i) {
        simulation.step();
        const double angle = w * simulation.time();
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        maxRadiusError = std::max(maxRadiusError,
                                  std::abs(separation(simulation.bodies()) / d - 1.0));
        for (std::size_t b = 0; b < 2; ++b) {
            const Vector3& p0 = initial[b].position; // Sobre el eje x.
            const Vector3 exact{p0.x * c, p0.x * s, 0.0};
            maxPositionError = std::max(
                maxPositionError, (simulation.bodies()[b].position - exact).magnitude() / d);
        }
    }

    // Los errores de Velocity Verlet escalan como (w dt)^2 ~ 2,5e-8 aquí. Una
    // tolerancia de 1e-6 de la distancia Tierra-Luna (~384 m) es 40 veces esa
    // estimación, mientras que un integrador de primer orden (p. ej., Euler
    // explícito) fallaría por ~1e-3, y una fuerza o velocidad inicial
    // equivocada, por mucho más.
    report("max |r / d - 1| en un periodo", maxRadiusError, 1e-6);
    report("max |x - x_exacta| / d en un periodo", maxPositionError, 1e-6);
    CHECK_LE(maxRadiusError, 1e-6);
    CHECK_LE(maxPositionError, 1e-6);
    // Ninguna fuerza tiene componente z, así que el movimiento sigue siendo exactamente plano.
    CHECK(simulation.bodies()[0].position.z == 0.0 && simulation.bodies()[1].position.z == 0.0);
}

// Máxima desviación relativa de la energía mecánica respecto de su valor inicial.
double maxRelativeEnergyError(std::vector<Body> bodies, double dt, double duration) {
    Simulation simulation{std::move(bodies), dt};
    const double e0 = gravitas::mechanicalEnergy(simulation.bodies());
    const auto steps = static_cast<std::uint64_t>(std::llround(duration / dt));
    double maxError = 0.0;
    for (std::uint64_t i = 0; i < steps; ++i) {
        simulation.step();
        const double e = gravitas::mechanicalEnergy(simulation.bodies());
        maxError = std::max(maxError, std::abs((e - e0) / e0));
    }
    return maxError;
}

void energyConservation() {
    // Velocity Verlet es simpléctico: el error de energía oscila y se mantiene
    // acotado en O((w dt)^2) ~ 2,5e-8 en lugar de derivar. En la órbita
    // circular, el radio y la rapidez se mantienen casi constantes, así que en
    // la práctica el error es mucho menor; 1e-8 sigue siendo lo bastante
    // holgado para ser robusto y lo bastante estricto para rechazar Euler
    // explícito, cuyo error aquí sería ~1e-3.
    const double circular = maxRelativeEnergyError(scenarios::makeEarthMoon(), 60.0,
                                                   27.0 * kSecondsPerDay);
    report("circular, dt = 60 s, 27 días: max |dE / E0|", circular, 1e-8);
    CHECK_LE(circular, 1e-8);

    // La órbita excéntrica pasa por el periápside a ~1/3 de la distancia
    // inicial, donde el movimiento es más rápido y el error, mayor.
    const double eccentric = maxRelativeEnergyError(makeEccentricEarthMoon(0.7), 60.0,
                                                    27.0 * kSecondsPerDay);
    report("excéntrica (e ~ 0,51), dt = 60 s, 27 días: max |dE / E0|", eccentric, 1e-6);
    CHECK_LE(eccentric, 1e-6);
}

void timeStepConvergence() {
    // Velocity Verlet es de segundo orden: reducir dt a la mitad debe dividir
    // el error por ~4. Un método de primer orden daría ~2; un error de
    // programación suele romper el patrón por completo. Se mide en la órbita
    // excéntrica durante 10 días.
    const double duration = 10.0 * kSecondsPerDay;

    auto finalMoonPosition = [duration](double dt) {
        Simulation simulation{makeEccentricEarthMoon(0.7), dt};
        simulation.step(static_cast<std::uint64_t>(std::llround(duration / dt)));
        return simulation.bodies()[1].position;
    };

    // Solución de referencia con un paso 64 veces menor que el más grueso.
    const Vector3 reference = finalMoonPosition(7.5);
    const double d = scenarios::kEarthMoonDistance;
    const std::array<double, 3> steps{480.0, 240.0, 120.0};
    std::array<double, 3> positionErrors{};
    std::array<double, 3> energyErrors{};
    for (std::size_t i = 0; i < steps.size(); ++i) {
        positionErrors[i] = (finalMoonPosition(steps[i]) - reference).magnitude() / d;
        energyErrors[i] = maxRelativeEnergyError(makeEccentricEarthMoon(0.7), steps[i], duration);
        std::cout << "    dt = " << steps[i] << " s: error de posición " << positionErrors[i]
                  << ", error máximo de energía " << energyErrors[i] << '\n';
    }

    for (std::size_t i = 0; i + 1 < steps.size(); ++i) {
        const double positionRatio = positionErrors[i] / positionErrors[i + 1];
        const double energyRatio = energyErrors[i] / energyErrors[i + 1];
        std::cout << "    cociente dt=" << steps[i] << " / dt=" << steps[i + 1]
                  << ": posición " << positionRatio << ", energía " << energyRatio
                  << " (esperado ~4)\n";
        // [3,5; 4,5] acepta el segundo orden con sus correcciones de orden
        // superior y excluye el comportamiento de primer (2) y tercer (8) orden.
        CHECK(positionRatio > 3.5 && positionRatio < 4.5);
        CHECK(energyRatio > 3.5 && energyRatio < 4.5);
    }
    // Los pasos más pequeños deben ser realmente más precisos.
    CHECK(positionErrors[2] < positionErrors[1] && positionErrors[1] < positionErrors[0]);
}

// --- Momento angular ----------------------------------------------------------------
//
// Con fuerzas internas centrales, Velocity Verlet conserva el momento angular
// total EXACTAMENTE en aritmética exacta, con cualquier paso temporal. El paso
// equivale a tres transformaciones, y ninguna cambia L:
//   - medio impulso v += 1/2 a dt: cambia L en 1/2 dt suma m_i r_i x a_i, que es
//     nulo porque cada par aporta m_i m_j (r_i - r_j) x g_ij = 0, con g_ij
//     paralelo a r_j - r_i (fuerza central);
//   - deriva r += v dt con la velocidad a medio paso: (r + v dt) x v = r x v;
//   - el segundo medio impulso, igual que el primero.
// Solo queda el redondeo, de ~1e-16 relativo por operación.

constexpr double kStepsPerDay60s = kSecondsPerDay / 60.0;

void angularMomentumConservation() {
    // Órbita excéntrica (e ~ 0,51) durante 27 días con dt = 60 s: 38 880 pasos.
    // Escalar ambas velocidades deja la órbita en el plano xy, así que L solo
    // tiene componente z.
    const double eccentric = maxRelativeAngularMomentumDrift(
        makeEccentricEarthMoon(0.7), 60.0, static_cast<std::uint64_t>(27.0 * kStepsPerDay60s));

    // Tres cuerpos en 3D con momento lineal neto durante 14 días: L tiene las
    // tres componentes y se mide respecto del origen, un punto fijo que el
    // sistema abandona; aun así se conserva porque el sistema está aislado.
    const double drifting = maxRelativeAngularMomentumDrift(
        makeDriftingThreeBody(), 60.0, static_cast<std::uint64_t>(14.0 * kStepsPerDay60s));

    // La tolerancia admite la acumulación lineal del redondeo, el peor caso:
    // ~40 000 pasos x unas pocas unidades de 1e-16. Un integrador que no
    // conserve L se desvía ~1e-3 en estos escenarios (ver el control con Euler
    // explícito más abajo), muchos órdenes de magnitud por encima.
    constexpr double kTolerance = 1e-11;
    report("excéntrica, dt = 60 s, 27 días: max |L - L0| / suma m|r x v|", eccentric, kTolerance);
    report("3 cuerpos en 3D con deriva, dt = 60 s, 14 días: max |L - L0| / suma m|r x v|",
           drifting, kTolerance);
    CHECK_LE(eccentric, kTolerance);
    CHECK_LE(drifting, kTolerance);
}

// Avanza los cuerpos con Euler explícito (r += v dt, v += a dt), un método de
// primer orden que NO conserva el momento angular: cada paso cambia L en
// dt^2 suma m_i v_i x a_i. Solo sirve como control negativo de la prueba.
double eulerRelativeAngularMomentumDrift(std::vector<Body> bodies, double dt,
                                         std::uint64_t steps) {
    const Vector3 l0 = gravitas::angularMomentum(bodies);
    const double scale = angularMomentumScale(bodies);
    for (std::uint64_t i = 0; i < steps; ++i) {
        gravitas::computeAccelerations(bodies);
        for (Body& body : bodies) {
            body.position += body.velocity * dt;
            body.velocity += body.acceleration * dt;
        }
    }
    return (gravitas::angularMomentum(bodies) - l0).magnitude() / scale;
}

void angularMomentumIsIndependentOfTimeStep() {
    // Con dt = 1 h, 60 veces el paso habitual, la energía ya no se conserva
    // bien (su error crece como dt^2), pero el momento angular sigue
    // conservándose al nivel del redondeo: es una propiedad estructural del
    // integrador, no una cuestión de precisión.
    const double duration = 27.0 * kSecondsPerDay;
    constexpr double kCoarseStep = 3600.0;
    const auto coarseSteps = static_cast<std::uint64_t>(duration / kCoarseStep);
    const double angular =
        maxRelativeAngularMomentumDrift(makeEccentricEarthMoon(0.7), kCoarseStep, coarseSteps);
    const double energy = maxRelativeEnergyError(makeEccentricEarthMoon(0.7), kCoarseStep, duration);
    std::cout << "    excéntrica, dt = 1 h, 27 días: max |dE / E0| = " << energy
              << " (solo informativo)\n";
    report("excéntrica, dt = 1 h, 27 días: max |L - L0| / suma m|r x v|", angular, 1e-12);
    CHECK_LE(angular, 1e-12);

    // Control negativo: con el mismo escenario y dt = 60 s, Euler explícito
    // cambia L en ~(w dt)^2 por paso, acumulado durante 38 880 pasos. Si la
    // prueba de conservación no lo distinguiera de Velocity Verlet, no
    // demostraría nada.
    const double euler = eulerRelativeAngularMomentumDrift(
        makeEccentricEarthMoon(0.7), 60.0, static_cast<std::uint64_t>(27.0 * kStepsPerDay60s));
    std::cout << "    control, Euler explícito, dt = 60 s, 27 días: |L - L0| / suma m|r x v|: "
              << euler << " (debe superar 1e-6)\n";
    CHECK(euler > 1e-6);
}

} // namespace

int main() {
    constexpr std::array tests{
        gravitas::test::TestCase{"Simulación: rechaza pasos temporales no válidos", rejectsInvalidTimeStep},
        gravitas::test::TestCase{"Simulación: el estado inicial es coherente", initialStateIsConsistent},
        gravitas::test::TestCase{"Simulación: step hace avanzar el tiempo", stepAdvancesTime},
        gravitas::test::TestCase{"Velocity Verlet: el paso coincide con sus fórmulas", verletStepMatchesFormulas},
        gravitas::test::TestCase{"Binaria circular: condiciones iniciales", circularBinaryInitialConditions},
        gravitas::test::TestCase{"Conservación del momento lineal", momentumConservation},
        gravitas::test::TestCase{"Estabilidad de la órbita circular", circularOrbitStability},
        gravitas::test::TestCase{"Conservación de la energía mecánica", energyConservation},
        gravitas::test::TestCase{"Convergencia con el paso temporal (segundo orden)", timeStepConvergence},
        gravitas::test::TestCase{"Conservación del momento angular", angularMomentumConservation},
        gravitas::test::TestCase{"Momento angular: conservación independiente del paso temporal", angularMomentumIsIndependentOfTimeStep},
    };
    return gravitas::test::runTests(tests);
}
