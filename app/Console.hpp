#pragma once

// Utilidades de consola compartidas por los programas de línea de comandos de
// GRAVITAS. Su texto para el usuario está en español y codificado en UTF-8.

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace gravitas::console {

#ifdef _WIN32
// Cambia la consola de Windows a UTF-8 mientras se ejecuta el programa, para
// que el texto con tildes se muestre correctamente, y restaura la página de
// códigos anterior al salir para dejar la terminal del usuario como estaba.
class Utf8Console {
public:
    Utf8Console() : previous_{GetConsoleOutputCP()} { SetConsoleOutputCP(CP_UTF8); }
    ~Utf8Console() {
        if (previous_ != 0) { // 0: no hay consola asociada (p. ej., salida redirigida).
            SetConsoleOutputCP(previous_);
        }
    }
    Utf8Console(const Utf8Console&) = delete;
    Utf8Console& operator=(const Utf8Console&) = delete;

private:
    UINT previous_;
};
#else
class Utf8Console {}; // Las demás plataformas usan terminales UTF-8 por defecto.
#endif

} // namespace gravitas::console
