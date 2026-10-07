#pragma once

#include <span>

#include "gravitas/Body.hpp"
#include "gravitas/Vector3.hpp"

namespace gravitas {

// Cantidades conservadas de un sistema gravitacional aislado. Se usan para
// comprobar la calidad de una simulación; ninguna modifica los cuerpos.

/// Energía cinética total en julios: suma de 1/2 m |v|^2.
[[nodiscard]] double kineticEnergy(std::span<const Body> bodies) noexcept;

/// Energía mecánica total en julios: cinética más energía potencial
/// gravitatoria (ver gravitationalPotentialEnergy()).
[[nodiscard]] double mechanicalEnergy(std::span<const Body> bodies) noexcept;

/// Momento lineal total en kg m/s: suma de m v.
[[nodiscard]] Vector3 totalMomentum(std::span<const Body> bodies) noexcept;

/// Momento angular total respecto del origen de coordenadas, en kg m^2/s:
///
///     L = suma de m_i (r_i x v_i)
///
/// Depende del punto de referencia: respecto de un punto c vale L - c x P,
/// con P = totalMomentum(). Si P = 0, no depende del punto elegido.
///
/// En un sistema aislado con fuerzas internas centrales (como la gravedad
/// newtoniana) se conserva respecto de cualquier punto fijo, también cuando
/// P != 0.
[[nodiscard]] Vector3 angularMomentum(std::span<const Body> bodies) noexcept;

/// Posición media ponderada por la masa, en metros. Devuelve el vector cero
/// para un conjunto vacío de cuerpos.
[[nodiscard]] Vector3 centerOfMass(std::span<const Body> bodies) noexcept;

} // namespace gravitas
