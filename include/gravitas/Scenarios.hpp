#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gravitas/Body.hpp"

namespace gravitas::scenarios {

// Unidades astronómicas expresadas en SI.
inline constexpr double kSolarMass = 1.98847e30;            // kg (GM_sol / G, IAU 2015)
inline constexpr double kParsec = 3.0856775814913673e16;    // m (IAU 2015)

// Parámetros simplificados de la Tierra y la Luna (unidades SI). Es un sistema
// idealizado de dos cuerpos, no una efeméride.
inline constexpr double kEarthMass = 5.972e24;          // kg
inline constexpr double kMoonMass = 7.342e22;           // kg
inline constexpr double kEarthMoonDistance = 3.844e8;   // m

/// Dos cuerpos en órbita circular alrededor de su centro de masas común.
///
/// - El centro de masas está en el origen y en reposo.
/// - Ambos cuerpos parten del eje x: el primario en (-r1, 0, 0) y el
///   secundario en (+r2, 0, 0), con r1 = d m2 / M, r2 = d m1 / M y
///   M = m1 + m2.
/// - Ambos giran en sentido antihorario en el plano xy con la misma velocidad
///   angular w = sqrt(G M / d^3): v1 = w r1 según -y, v2 = w r2 según +y.
///   Por tanto, el momento lineal total es nulo (m1 v1 = m2 v2).
///
/// Devuelve {primario, secundario}.
/// @throws std::invalid_argument si una masa no es válida (ver Body) o la
///         separación no es finita y positiva.
[[nodiscard]] std::vector<Body> makeCircularBinary(double primaryMass,
                                                   double secondaryMass,
                                                   double separation);

/// Periodo orbital en segundos de la binaria circular construida por
/// makeCircularBinary(): T = 2 pi sqrt(d^3 / (G M)).
/// @throws std::invalid_argument en las mismas condiciones.
[[nodiscard]] double circularBinaryPeriod(double primaryMass,
                                          double secondaryMass,
                                          double separation);

/// La Tierra y la Luna en una órbita circular idealizada: {Tierra, Luna}.
[[nodiscard]] std::vector<Body> makeEarthMoon();

/// Cúmulo estelar que sigue el modelo de Plummer, el sistema de prueba estándar
/// de los códigos de N cuerpos. Su densidad es
///
///     rho(r) = 3 M / (4 pi a^3) (1 + r^2 / a^2)^(-5/2)
///
/// con masa total M y radio de escala a. El cúmulo está en equilibrio virial
/// (2K = -U), su radio de media masa es a / sqrt(2^(2/3) - 1) ~ 1,305 a y su
/// energía total es E = -(3 pi / 64) G M^2 / a.
///
/// - `count` cuerpos de igual masa M / count.
/// - Las posiciones y velocidades se muestrean de la función de distribución
///   exacta (Aarseth, Hénon y Wielen 1974): los radios invirtiendo el perfil
///   de masa acumulada, las rapideces por muestreo de rechazo y las
///   direcciones de forma isótropa. La distribución no se trunca a radios
///   grandes.
/// - El resultado se desplaza para que el centro de masas quede en el origen y
///   en reposo (momento total nulo).
/// - Determinista: la misma semilla produce los mismos cuerpos. Los números
///   aleatorios provienen de std::mt19937_64, cuya salida está totalmente
///   especificada por el estándar de C++; aun así, las bibliotecas estándar
///   pueden diferir en los últimos bits de std::sqrt, std::pow, std::sin o
///   std::cos.
///
/// No hay suavizado, así que nada impide que dos estrellas se muestreen muy
/// cerca una de otra.
///
/// @throws std::invalid_argument si count es cero, la masa de cada cuerpo no es
///         válida (ver Body) o el radio de escala no es finito y positivo.
[[nodiscard]] std::vector<Body> makePlummerSphere(std::size_t count, double totalMass,
                                                  double scaleRadius, std::uint64_t seed);

} // namespace gravitas::scenarios
