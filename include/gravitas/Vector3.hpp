#pragma once

#include <cassert>
#include <cmath>
#include <ostream>

namespace gravitas {

/// Vector cartesiano tridimensional con componentes de doble precisión.
///
/// Vector3 es un tipo valor agregado simple: es trivialmente copiable y nunca
/// reserva memoria. No lleva información de unidades; la unidad de cada
/// componente la define el contexto en que se usa el vector (ver Body).
///
/// Construcción:
///   Vector3{}           -> (0, 0, 0)
///   Vector3{1, 2, 3}    -> (1, 2, 3)
///   Vector3{.x = 1}     -> (1, 0, 0)
///
/// Política de división por cero:
///   Dividir un vector por un escalar igual a cero es una violación de
///   precondición. Se detecta con assert() en las compilaciones donde NDEBUG no
///   está definido (Debug). En las compilaciones donde NDEBUG está definido
///   (Release) no se comprueba nada y el resultado sigue la aritmética
///   IEEE 754: las componentes pasan a ser +inf, -inf o NaN. Quien pueda
///   encontrarse legítimamente con un divisor nulo debe comprobarlo antes de
///   dividir.
///
/// Rango numérico:
///   magnitude() se calcula como sqrt(x*x + y*y + z*z). La suma intermedia
///   desborda para componentes mayores que aproximadamente 1e154 y se anula por
///   subdesbordamiento para componentes menores que aproximadamente 1e-154.
///   Ambos límites están muy lejos de los rangos SI que GRAVITAS pretende
///   manejar (p. ej., 1 año luz ~ 9,5e15 m).
struct Vector3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    constexpr Vector3& operator+=(const Vector3& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Vector3& operator-=(const Vector3& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    constexpr Vector3& operator*=(double scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    /// Precondición: scalar != 0 (ver la política de división por cero arriba).
    constexpr Vector3& operator/=(double scalar) noexcept {
        assert(scalar != 0.0 && "gravitas::Vector3: división por cero");
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    /// Producto escalar.
    [[nodiscard]] constexpr double dot(const Vector3& rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    /// Producto vectorial, dextrógiro: X.cross(Y) == Z.
    [[nodiscard]] constexpr Vector3 cross(const Vector3& rhs) const noexcept {
        return {y * rhs.z - z * rhs.y,
                z * rhs.x - x * rhs.z,
                x * rhs.y - y * rhs.x};
    }

    /// Longitud euclídea al cuadrado. Más barata que magnitude(); es preferible
    /// cuando solo se comparan longitudes.
    [[nodiscard]] constexpr double magnitudeSquared() const noexcept {
        return dot(*this);
    }

    /// Longitud euclídea.
    [[nodiscard]] double magnitude() const noexcept {
        return std::sqrt(magnitudeSquared());
    }

    /// Devuelve un vector unitario con la misma dirección que este vector.
    ///
    /// Vectores nulos: si el módulo es exactamente cero, se devuelve el vector
    /// cero (0, 0, 0). No se lanza ninguna excepción, no salta ningún assert y
    /// no se produce NaN. Un vector cuyo módulo al cuadrado se anula por
    /// subdesbordamiento (todas las componentes por debajo de aproximadamente
    /// 1e-154) se trata como nulo. Cualquier otro vector no nulo, por pequeño
    /// que sea, se normaliza con normalidad.
    ///
    /// Las componentes no finitas (inf o NaN) producen resultados no finitos.
    [[nodiscard]] Vector3 normalized() const noexcept {
        const double length = magnitude();
        if (length == 0.0) {
            return {};
        }
        return {x / length, y / length, z / length};
    }

    friend constexpr bool operator==(const Vector3&, const Vector3&) noexcept = default;
};

[[nodiscard]] constexpr Vector3 operator+(Vector3 lhs, const Vector3& rhs) noexcept {
    return lhs += rhs;
}

[[nodiscard]] constexpr Vector3 operator-(Vector3 lhs, const Vector3& rhs) noexcept {
    return lhs -= rhs;
}

[[nodiscard]] constexpr Vector3 operator-(const Vector3& v) noexcept {
    return {-v.x, -v.y, -v.z};
}

[[nodiscard]] constexpr Vector3 operator*(Vector3 v, double scalar) noexcept {
    return v *= scalar;
}

[[nodiscard]] constexpr Vector3 operator*(double scalar, Vector3 v) noexcept {
    return v *= scalar;
}

/// Precondición: scalar != 0 (ver la política de división por cero de Vector3).
[[nodiscard]] constexpr Vector3 operator/(Vector3 v, double scalar) noexcept {
    return v /= scalar;
}

/// Escribe el vector como "(x, y, z)" con el formato actual del flujo.
inline std::ostream& operator<<(std::ostream& os, const Vector3& v) {
    return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}

} // namespace gravitas
