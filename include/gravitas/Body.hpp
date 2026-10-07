#pragma once

#include <limits>
#include <stdexcept>

#include "gravitas/Vector3.hpp"

namespace gravitas {

/// Cuerpo físico puntual.
///
/// Unidades (SI):
///   masa          kilogramos                   (kg)
///   posición      metros                       (m)
///   velocidad     metros por segundo           (m/s)
///   aceleración   metros por segundo al cuadrado (m/s^2)
///
/// Invariante: la masa es finita y estrictamente positiva. La impone el
/// constructor, por eso la masa es privada y de solo lectura. Los vectores
/// cinemáticos no tienen invariante y son miembros de datos públicos.
///
/// La aceleración empieza en cero. Es solo almacenamiento de estado: nada en
/// este tipo calcula fuerzas ni hace avanzar el cuerpo en el tiempo.
class Body {
public:
    Vector3 position{};
    Vector3 velocity{};
    Vector3 acceleration{};

    /// @param mass             Masa en kg; debe cumplir isValidMass().
    /// @param initialPosition  Posición en m.
    /// @param initialVelocity  Velocidad en m/s.
    /// @throws std::invalid_argument si la masa es nula, negativa, NaN o
    ///         infinita.
    constexpr explicit Body(double mass,
                            const Vector3& initialPosition = {},
                            const Vector3& initialVelocity = {})
        : position{initialPosition},
          velocity{initialVelocity},
          mass_{validatedMass(mass)} {}

    /// Masa en kilogramos. Siempre finita y estrictamente positiva.
    [[nodiscard]] constexpr double mass() const noexcept { return mass_; }

    /// Verdadero si el valor es un número finito y estrictamente positivo.
    /// Rechaza el cero (incluido -0.0), los valores negativos, NaN y
    /// +/-infinito.
    [[nodiscard]] static constexpr bool isValidMass(double mass) noexcept {
        // Toda comparación con NaN es falsa, así que NaN también se rechaza aquí.
        return mass > 0.0 && mass <= std::numeric_limits<double>::max();
    }

private:
    double mass_;

    static constexpr double validatedMass(double mass) {
        if (!isValidMass(mass)) {
            throw std::invalid_argument(
                "gravitas::Body: la masa debe ser finita y estrictamente positiva (kg)");
        }
        return mass;
    }
};

} // namespace gravitas
