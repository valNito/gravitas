#pragma once

#include <span>

#include "gravitas/Body.hpp"

namespace gravitas {

/// Constante de gravitación newtoniana G, en m^3 kg^-1 s^-2 (CODATA 2018).
inline constexpr double kGravitationalConstant = 6.67430e-11;

/// Sobrescribe la aceleración de cada cuerpo con la aceleración gravitatoria
/// newtoniana producida por todos los demás cuerpos (suma directa por pares,
/// O(N^2)):
///
///     a_i = suma sobre j != i de  G m_j (x_j - x_i) / |x_j - x_i|^3
///
/// Cada par se evalúa una sola vez y se aplica a ambos cuerpos con signos
/// opuestos, de modo que m_i a_i + m_j a_j = 0 salvo redondeo (tercera ley de
/// Newton).
///
/// Cuerpos coincidentes: un par a distancia exactamente cero no tiene una
/// fuerza definida. Se omite y no aporta nada a ninguno de los dos cuerpos, así
/// que no se produce división por cero, inf ni NaN. Los cuerpos que solo están
/// muy cerca no reciben ningún trato especial (no hay suavizado): la
/// aceleración crece como 1/r^2 y puede hacerse arbitrariamente grande.
void computeAccelerations(std::span<Body> bodies) noexcept;

/// Energía potencial gravitatoria total en julios:
///
///     U = - suma sobre i < j de  G m_i m_j / |x_j - x_i|
///
/// Los pares a distancia cero se omiten, en coherencia con computeAccelerations().
[[nodiscard]] double gravitationalPotentialEnergy(std::span<const Body> bodies) noexcept;

} // namespace gravitas
