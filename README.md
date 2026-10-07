# GRAVITAS

GRAVITAS es un motor de simulación gravitacional de N cuerpos escrito en C++20.

## Objetivo

El objetivo a largo plazo es un motor físico autónomo para la simulación
gravitacional de N cuerpos, utilizable desde una aplicación de consola y, más
adelante, desde una interfaz gráfica 3D. El proyecto se construye en etapas
pequeñas y verificadas.

## Estado actual

**Etapa 3.1 — Conservación del momento angular.**
GRAVITAS integra el movimiento de cuerpos bajo la gravedad newtoniana, incluye
una aplicación de consola que simula un sistema Tierra-Luna idealizado y mide el
costo de su gravedad directa O(N²) en cúmulos estelares de hasta 20 000 cuerpos,
como referencia para optimizaciones futuras (etapa 2). La etapa 3 valida la
física de las simulaciones; la 3.1 añade el momento angular y verifica su
conservación.

Implementado:

- **Matemáticas y cuerpos (etapa 1).** `Vector3`: vector 3D con componentes
  `double` (aritmética, productos escalar y vectorial, módulo, normalización).
  `Body`: masa puntual con posición, velocidad y aceleración en unidades SI
  (kg, m, m/s, m/s²); la masa debe ser finita y estrictamente positiva.
- **Gravedad** (`Gravity.hpp`). Ley de gravitación universal de Newton con
  G = 6,67430 × 10⁻¹¹ m³ kg⁻¹ s⁻², evaluada por suma directa por pares (O(N²)).
  Cada par se calcula una sola vez y se aplica con signos opuestos, de modo que
  la tercera ley de Newton se cumple salvo redondeo. También calcula la energía
  potencial gravitatoria.
- **Integrador Velocity Verlet** (`VelocityVerlet.hpp`). De segundo orden,
  simpléctico, con paso temporal configurable:
  `x(n+1) = x(n) + v(n) dt + ½ a(n) dt²` y
  `v(n+1) = v(n) + ½ (a(n) + a(n+1)) dt`.
  La aceleración a(n+1) calculada al final de un paso se reutiliza como a(n)
  del siguiente, así que las fuerzas se evalúan una vez por paso.
- **Estado de la simulación** (`Simulation.hpp`). Es dueña de los cuerpos, del
  paso temporal y del tiempo transcurrido. Calcula las aceleraciones iniciales y
  hace avanzar el sistema paso a paso.
- **Diagnósticos** (`Diagnostics.hpp`). Energía cinética y mecánica, momento
  lineal total, momento angular total `L = Σ mᵢ (rᵢ × vᵢ)` respecto del origen
  y centro de masas.
- **Conservación del momento angular (etapa 3.1).** Con fuerzas centrales,
  Velocity Verlet conserva L exactamente salvo redondeo, con cualquier paso
  temporal: las pruebas miden una variación relativa de ~1e-14 en 27 días
  (órbita excéntrica, e ≈ 0,51) y en un sistema 3D de tres cuerpos con momento
  lineal neto, frente a ~6e-3 con Euler explícito en el mismo escenario.
- **Escenarios** (`Scenarios.hpp`). Una órbita circular de dos cuerpos alrededor
  del centro de masas (centro de masas en el origen y en reposo, momento total
  nulo) y la configuración Tierra-Luna construida a partir de ella. Una
  **esfera de Plummer**: un cúmulo estelar de N cuerpos de igual masa muestreado
  de la función de distribución exacta de Plummer, en su sistema del centro de
  masas, reproducible a partir de una semilla.
- **Aplicación de consola** (`gravitas`). Ejecuta la simulación Tierra-Luna e
  imprime las posiciones día a día, junto con diagnósticos de energía, momento
  lineal y momento angular.
- **Prueba de rendimiento** (`gravitas_benchmark`). Mide el tiempo de la
  evaluación directa O(N²) de las fuerzas en cúmulos de Plummer de 100 a 20 000
  cuerpos.

Aún no implementado: interfaz gráfica, OpenGL, Barnes-Hut / octree, suavizado
gravitacional (*softening*), paralelización y otros integradores. Se pueden
generar cúmulos grandes y evaluar sus fuerzas, pero las simulaciones largas de
esos cúmulos todavía no son prácticas (ver la prueba de rendimiento) ni precisas
durante los encuentros cercanos (no hay suavizado).

### Comportamiento documentado en casos límite

- **Cuerpos coincidentes.** Un par de cuerpos a distancia exactamente cero no
  tiene una fuerza gravitatoria definida: el par se omite (ni fuerza ni energía
  potencial entre ellos), de modo que no se produce división por cero, inf ni
  NaN. Los cuerpos que solo están muy cerca no reciben ningún trato especial; no
  hay suavizado.
- **Paso temporal.** `Simulation` rechaza un paso temporal nulo, negativo, NaN o
  infinito con `std::invalid_argument`.
- **Normalizar un vector nulo** devuelve el vector cero `(0, 0, 0)`; no lanza
  excepciones ni produce NaN. Un vector cuyo módulo al cuadrado se anula por
  subdesbordamiento (componentes por debajo de aproximadamente 1e-154) se trata
  como nulo.
- **Dividir un vector por cero** es una violación de precondición. Dispara un
  `assert` en las compilaciones Debug; en las compilaciones Release el resultado
  sigue IEEE 754 (componentes ±inf o NaN).

## Requisitos

- Un compilador C++20 (probado con MSVC 19.44 / Visual Studio 2022 Build Tools).
  GCC y Clang están configurados con un conjunto de advertencias equivalente,
  pero todavía no se han probado.
- CMake 3.21 o posterior.
- Sin dependencias de terceros.

## Compilación

La compilación produce la biblioteca principal `gravitas_core` (estática), la
aplicación de consola `gravitas`, la prueba de rendimiento `gravitas_benchmark`,
el ejemplo de la etapa 1 y los ejecutables de prueba.

Generadores multiconfiguración (Visual Studio, el predeterminado en Windows):

```sh
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release
```

Generadores de una sola configuración (Ninja, Makefiles), un árbol de
compilación por tipo:

```sh
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

Si no se indica `CMAKE_BUILD_TYPE`, las compilaciones de una sola configuración
usan `Release` por defecto.

Opciones de CMake:

| Opción                        | Valor por defecto   | Propósito                                     |
|-------------------------------|---------------------|-----------------------------------------------|
| `GRAVITAS_BUILD_EXAMPLES`     | ON (nivel superior) | Compilar `gravitas_example`                   |
| `GRAVITAS_BUILD_TESTS`        | ON (nivel superior) | Compilar las pruebas y habilitar CTest        |
| `GRAVITAS_WARNINGS_AS_ERRORS` | OFF                 | Tratar las advertencias como errores          |

Advertencias: `/W4 /permissive-` en MSVC; `-Wall -Wextra -Wpedantic -Wshadow
-Wconversion -Wsign-conversion` y otras en GCC/Clang. Todo el código fuente está
en UTF-8 (los comentarios y los mensajes están en español); en MSVC se compila
con `/utf-8`.

## Ejecución de la simulación Tierra-Luna

```sh
# Generador de Visual Studio
./build/Release/gravitas
# Generador de una sola configuración
./build-debug/gravitas
```

El escenario es un sistema idealizado de dos cuerpos, no una efeméride
astronómica:

| Parámetro          | Valor             |
|--------------------|-------------------|
| Masa de la Tierra  | 5,972 × 10²⁴ kg   |
| Masa de la Luna    | 7,342 × 10²² kg   |
| Distancia inicial  | 384 400 km        |
| Paso temporal      | 60 s              |
| Duración           | 27 días           |

Ambos cuerpos parten en una órbita circular alrededor de su centro de masas
común, que es el origen del sistema de referencia; la Tierra no está fija. El
programa imprime las posiciones x/y de la Tierra y la Luna en km desde el día 0
hasta el día 27, seguidas de la distancia inicial y final, la energía mecánica,
el error relativo de energía, el momento lineal total y el momento angular
inicial y final con su error relativo.

La salida de la consola está en español, codificada en UTF-8. En Windows, el
programa cambia la consola a UTF-8 mientras se ejecuta y restaura la página de
códigos anterior al terminar.

El ejemplo de la etapa 1 (`gravitas_example`) sigue disponible; solo ejercita
las operaciones vectoriales y no simula movimiento.

## Ejecución de la prueba de rendimiento

```sh
./build/Release/gravitas_benchmark
```

Compílala en Release: los tiempos en Debug no son representativos, y el
programa lo advierte cuando se compiló sin optimizaciones. Genera cúmulos de
Plummer (1 masa solar por estrella, radio de escala 1 pc, semilla 42) con
N = 100 a 20 000 e informa, para cada N, la mediana del tiempo de una evaluación
de fuerzas (al menos 3 ejecuciones y 0,5 s de medición), el costo por par y la
pendiente log-log entre tamaños consecutivos, que debería ser cercana a 2 para un
método O(N²). Termina con una proyección O(N²) a valores de N mayores. La
ejecución completa tarda unos 10 segundos.

Ejecución de referencia (MSVC 19.44, Release, un solo hilo):

| N      | Tiempo por evaluación | ns por par | Pendiente |
|--------|-----------------------|------------|-----------|
| 1 000  | 2,7 ms                | 5,5        | 2,02      |
| 10 000 | 0,32 s                | 6,5        | 2,05      |
| 20 000 | 1,25 s                | 6,3        | 1,95      |

Proyección: unos 31 s por evaluación para N = 100 000 y 52 min para
N = 1 000 000. Dos ejecuciones consecutivas difirieron en menos de un 4 %.

## Animación Tierra-Luna

`visualizations/earth_moon/animate.py` genera un MP4 (1920x1080, 60 fps) de la
simulación Tierra-Luna a partir de las posiciones de
`visualizations/earth_moon/positions.csv`, copiadas literalmente de la salida de
`gravitas`. Requiere Python 3, NumPy, Matplotlib y FFmpeg. Los fotogramas entre
los registros diarios son interpolaciones lineales solo para visualización.

```sh
cd visualizations/earth_moon
python animate.py
```

## Ejecución de las pruebas

Las pruebas usan un pequeño arnés propio (sin framework externo) y están
registradas en CTest:

```sh
# Generador de Visual Studio
ctest --test-dir build -C Debug --output-on-failure
# Generador de una sola configuración
ctest --test-dir build-debug --output-on-failure
```

| Ejecutable de prueba | Cubre                                                        |
|----------------------|--------------------------------------------------------------|
| `test_vector3`       | Aritmética vectorial, productos, módulos, normalización      |
| `test_body`          | Construcción de cuerpos y validación de la masa              |
| `test_gravity`       | Módulo y dirección de la aceleración, tercera ley, cuerpos coincidentes, energía potencial |
| `test_diagnostics`   | Momento angular: valores exactos, aditividad, dependencia del punto de referencia (L − c × P) y binaria circular frente a la solución analítica μ d² ω |
| `test_simulation`    | Validación del paso temporal, paso de Verlet, condiciones iniciales circulares, conservación del momento lineal, de la energía y del momento angular (también con paso grueso, y con Euler explícito como control negativo), estabilidad orbital frente a la solución analítica, convergencia de segundo orden |
| `test_scenarios`     | Esfera de Plummer: validación de parámetros, determinismo, masas iguales, sistema del centro de masas, radio de media masa, equilibrio virial, energía total e isotropía frente al modelo analítico |

Cada ejecutable de prueba también puede ejecutarse directamente para ver sus
casos de prueba individuales; `test_simulation` y `test_scenarios` además
imprimen los valores medidos junto a sus tolerancias.

## Estructura del proyecto

```
include/gravitas/   Cabeceras públicas de gravitas_core
src/                Fuentes compiladas de gravitas_core
app/                Programas de consola: simulación y prueba de rendimiento (solo presentación)
examples/           Programas de ejemplo
tests/              Pruebas automatizadas
visualizations/     Scripts de Python que representan resultados calculados por GRAVITAS
```

## Licencia

MIT. Ver [LICENSE](LICENSE).
