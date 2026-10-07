// Prueba de humo de gravitas_core: construye dos cuerpos y aplica operaciones
// vectoriales a su estado. No calcula fuerzas ni mueve los cuerpos.

#include <iomanip>
#include <iostream>

#include "Console.hpp"
#include "gravitas/Body.hpp"
#include "gravitas/Vector3.hpp"

int main() {
    using gravitas::Body;
    using gravitas::Vector3;

    [[maybe_unused]] const gravitas::console::Utf8Console utf8Console;

    // Valores aproximados de la Tierra y la Luna, unidades SI (kg, m, m/s).
    const Body earth{5.972e24, Vector3{0.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}};
    const Body moon{7.342e22, Vector3{3.844e8, 0.0, 0.0}, Vector3{0.0, 1.022e3, 0.0}};

    std::cout << std::setprecision(6);
    std::cout << "GRAVITAS — ejemplo de la etapa 1\n\n";

    std::cout << "Tierra: masa = " << earth.mass() << " kg\n"
              << "        posición = " << earth.position << " m\n"
              << "        velocidad = " << earth.velocity << " m/s\n";
    std::cout << "Luna:   masa = " << moon.mass() << " kg\n"
              << "        posición = " << moon.position << " m\n"
              << "        velocidad = " << moon.velocity << " m/s\n\n";

    const Vector3 displacement = moon.position - earth.position;
    const Vector3 direction = displacement.normalized();
    const Vector3 relativeVelocity = moon.velocity - earth.velocity;

    std::cout << "Operaciones vectoriales\n";
    std::cout << "  desplazamiento (luna - tierra)          = " << displacement << " m\n";
    std::cout << "  distancia |desplazamiento|              = " << displacement.magnitude() << " m\n";
    std::cout << "  distancia al cuadrado                   = " << displacement.magnitudeSquared() << " m²\n";
    std::cout << "  dirección unitaria                      = " << direction << '\n';
    std::cout << "  |dirección unitaria|                    = " << direction.magnitude() << '\n';
    std::cout << "  velocidad relativa                      = " << relativeVelocity << " m/s\n";
    std::cout << "  dirección · velocidad relativa          = " << direction.dot(relativeVelocity) << " m/s\n";
    std::cout << "  desplazamiento × velocidad relativa     = " << displacement.cross(relativeVelocity) << " m²/s\n";
    std::cout << "  punto medio (tierra + luna) / 2         = " << (earth.position + moon.position) / 2.0 << " m\n";
    std::cout << "  2 · velocidad relativa                  = " << 2.0 * relativeVelocity << " m/s\n";
    std::cout << "  vector cero normalizado                 = " << Vector3{}.normalized() << '\n';

    return 0;
}
