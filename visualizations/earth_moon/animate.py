"""GRAVITAS - Animación orbital Tierra-Luna.

Genera un MP4 de la Tierra y la Luna moviéndose alrededor de su baricentro
común, usando solo las posiciones calculadas por el motor GRAVITAS
(positions.csv: un registro por día simulado, en km, relativo al baricentro).

No se reintegra nada ni se sintetiza ninguna órbita. Los fotogramas entre dos
registros diarios son INTERPOLACIONES LINEALES SOLO PARA VISUALIZACIÓN; cada
fotograma que cae en un día entero muestra exactamente la posición registrada.

Uso:       python animate.py
Salida:    output/gravitas_earth_moon.mp4  (1920x1080, 60 fps, H.264)
Requiere:  Python 3, NumPy, Matplotlib y FFmpeg en el PATH.
"""

from __future__ import annotations

import csv
import math
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import matplotlib

matplotlib.use("Agg")  # Renderizado fuera de pantalla; no se abre ninguna ventana.

import matplotlib.pyplot as plt
from matplotlib import animation
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter, MultipleLocator

HERE = Path(__file__).resolve().parent
DATA_FILE = HERE / "positions.csv"
OUTPUT_FILE = HERE / "output" / "gravitas_earth_moon.mp4"

# --- Datos físicos (deben coincidir con el escenario de GRAVITAS) -------------
EARTH_MASS = 5.972e24  # kg
MOON_MASS = 7.342e22   # kg
EXPECTED_DAYS = np.arange(28)  # Registros de los días 0 .. 27.
DECIMALS = 3                   # Precisión de las posiciones registradas (km).

# --- Vídeo ---------------------------------------------------------------------
WIDTH, HEIGHT, DPI = 1920, 1080, 100
FPS = 60
FRAMES_PER_DAY = 40      # 27 días de movimiento en 18 s; los días enteros caen en fotogramas.
HOLD_START_FRAMES = 30   # 0,5 s mostrando el estado inicial.
HOLD_END_FRAMES = 120    # 2 s mostrando el estado final.

# --- Vistas (mismos datos, distintas escalas) -----------------------------------
MAIN_HALF_RANGE_KM = 420_000.0  # Órbita lunar completa (radio ~380 000 km).
ZOOM_HALF_RANGE_KM = 6_000.0    # Órbita de la Tierra alrededor del baricentro (~4700 km).

# --- Estilo ------------------------------------------------------------------------
BACKGROUND = "#04060d"
PANEL = "#060a16"
GRID = "#18213a"
SPINE = "#2a3556"
TEXT = "#e3e9f5"
MUTED = "#8c97b2"
EARTH_COLOR = "#2f7dff"
EARTH_EDGE = "#a9ccff"
EARTH_TRAIL = "#4aa3ff"
MOON_COLOR = "#d4d8e0"
MOON_TRAIL = "#b79cff"
BARYCENTER_COLOR = "#ffd54a"
RECORD_COLOR = "#f2f4f8"


# --- Formato numérico en español (el texto en pantalla está en español) ----------

def thousands(value: float) -> str:
    """Entero con espacio como separador de miles: 379731.6 -> '379 732'."""
    return f"{value:,.0f}".replace(",", " ")


def decimal(value: float, digits: int, width: int = 0) -> str:
    """Número en coma fija con coma decimal: 7.5 -> '7,50'."""
    return f"{value:{width}.{digits}f}".replace(".", ",")


@dataclass(frozen=True)
class Positions:
    """Posiciones registradas por GRAVITAS (km). Los arreglos son de solo lectura."""

    days: np.ndarray   # (N,) int
    earth: np.ndarray  # (N, 2)
    moon: np.ndarray   # (N, 2)
    tokens: list[list[str]]  # Coordenadas tal como están escritas en el CSV.


# ==============================================================================
# Carga y validación de los datos
# ==============================================================================

def load_positions(path: Path) -> Positions:
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.reader(line for line in f if not line.startswith("#")))
    header, records = rows[0], rows[1:]
    expected_header = ["day", "earth_x_km", "earth_y_km", "moon_x_km", "moon_y_km"]
    if header != expected_header:
        raise ValueError(f"unexpected CSV header {header}, expected {expected_header}")

    days = np.array([int(r[0]) for r in records])
    coords = np.array([[float(v) for v in r[1:]] for r in records])
    earth, moon = coords[:, 0:2].copy(), coords[:, 2:4].copy()
    for array in (days, earth, moon):
        array.flags.writeable = False  # Protección contra modificaciones accidentales.
    return Positions(days, earth, moon, [r[1:] for r in records])


def barycenter(earth: np.ndarray, moon: np.ndarray) -> np.ndarray:
    return (EARTH_MASS * earth + MOON_MASS * moon) / (EARTH_MASS + MOON_MASS)


def check(condition: bool, message: str) -> None:
    print(f"  [{'OK' if condition else 'FALLO'}] {message}")
    if not condition:
        raise SystemExit(f"Validación fallida: {message}")


def validate_data(p: Positions) -> None:
    print("Validación de datos")
    # 1. Número de registros.
    check(len(p.days) == 28, f"28 registros cargados (encontrados: {len(p.days)})")

    # 2. Días ordenados 0..27.
    check(np.array_equal(p.days, EXPECTED_DAYS), "días ordenados 0, 1, ..., 27")

    # 3. Coordenadas originales conservadas: cada valor leído se vuelve a
    #    imprimir como el texto exacto del CSV, así que al cargar no se perdió
    #    ni se alteró precisión.
    verbatim = all(f"{float(t):.{DECIMALS}f}" == t for row in p.tokens for t in row)
    check(verbatim, f"las {4 * len(p.tokens)} coordenadas coinciden exactamente con el "
                    "texto del CSV")

    # 4. Baricentro en el origen. Cada coordenada está redondeada a 3 decimales,
    #    es decir, desviada como mucho 0,0005 km; la media ponderada por la masa
    #    de dos de esos valores se desvía, por tanto, como mucho 0,0005 km por eje.
    tolerance = 0.5 * 10.0 ** -DECIMALS + 1e-12
    offsets = np.abs(barycenter(p.earth, p.moon))
    worst = offsets.max()
    check(worst <= tolerance,
          f"baricentro en (0, 0) dentro de la tolerancia de redondeo: desviación máxima "
          f"{decimal(worst * 1000, 3)} m por eje <= {decimal(tolerance * 1000, 3)} m")


# ==============================================================================
# Línea de tiempo e interpolación
# ==============================================================================

def build_timeline(last_day: int) -> np.ndarray:
    """Tiempo simulado (días) que muestra cada fotograma del vídeo."""
    motion = np.arange(last_day * FRAMES_PER_DAY + 1) / FRAMES_PER_DAY
    return np.concatenate([np.zeros(HOLD_START_FRAMES), motion,
                           np.full(HOLD_END_FRAMES, float(last_day))])


def interpolate(times: np.ndarray, days: np.ndarray, xy: np.ndarray) -> np.ndarray:
    """Interpolación lineal a trozos entre registros diarios (solo para visualización)."""
    return np.column_stack([np.interp(times, days, xy[:, 0]),
                            np.interp(times, days, xy[:, 1])])


def validate_interpolation(p: Positions, times: np.ndarray,
                           earth_frames: np.ndarray, moon_frames: np.ndarray,
                           earth_before: np.ndarray, moon_before: np.ndarray) -> None:
    print("Validación de la interpolación")
    check(bool(np.all(np.diff(times) >= 0.0)) and times[0] == 0.0 and times[-1] == p.days[-1],
          f"los tiempos de los fotogramas crecen de 0 a {p.days[-1]} días")

    # 6. Cada día entero se muestra en un fotograma cuya posición es el propio registro.
    record_frames = HOLD_START_FRAMES + p.days * FRAMES_PER_DAY
    check(np.array_equal(times[record_frames], p.days.astype(float)),
          "cada día registrado cae exactamente en un fotograma")
    check(np.array_equal(earth_frames[record_frames], p.earth)
          and np.array_equal(moon_frames[record_frames], p.moon),
          "los fotogramas de los días registrados reproducen las posiciones bit a bit")
    check(np.array_equal(p.earth, earth_before) and np.array_equal(p.moon, moon_before),
          "los datos originales no cambian tras la interpolación")


def validate_scales(axes: dict[str, plt.Axes], p: Positions) -> None:
    """5. Misma escala en ambos ejes de cada panel y extensión correcta de las vistas."""
    print("Validación de escalas")
    for name, ax in axes.items():
        origin = ax.transData.transform((0.0, 0.0))
        px_per_km_x = ax.transData.transform((1.0, 0.0))[0] - origin[0]
        px_per_km_y = ax.transData.transform((0.0, 1.0))[1] - origin[1]
        check(math.isclose(px_per_km_x, px_per_km_y, rel_tol=1e-9),
              f"panel {name}: misma escala en x e y "
              f"({f'{px_per_km_x:.6g}'.replace('.', ',')} px/km)")

    moon_extent = np.abs(p.moon).max()
    earth_extent = np.abs(p.earth).max()
    check(moon_extent < MAIN_HALF_RANGE_KM,
          f"el panel principal muestra toda la órbita lunar ({thousands(moon_extent)} km < "
          f"{thousands(MAIN_HALF_RANGE_KM)} km)")
    check(earth_extent < ZOOM_HALF_RANGE_KM,
          f"el panel de acercamiento muestra toda la órbita terrestre "
          f"({thousands(earth_extent)} km < {thousands(ZOOM_HALF_RANGE_KM)} km)")
    print(f"  aumento del acercamiento: ×{MAIN_HALF_RANGE_KM / ZOOM_HALF_RANGE_KM:.0f}")


# ==============================================================================
# Figura
# ==============================================================================

def style_panel(ax: plt.Axes, half_range: float, tick_step: float,
                formatter: FuncFormatter, unit: str) -> None:
    ax.set_facecolor(PANEL)
    ax.set_xlim(-half_range, half_range)
    ax.set_ylim(-half_range, half_range)
    ax.set_aspect("equal", adjustable="box")
    for axis in (ax.xaxis, ax.yaxis):
        axis.set_major_locator(MultipleLocator(tick_step))
        axis.set_major_formatter(formatter)
    ax.tick_params(colors=MUTED, labelsize=11, length=3, width=0.6)
    ax.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for spine in ax.spines.values():
        spine.set_color(SPINE)
        spine.set_linewidth(0.8)
    ax.set_xlabel(f"x [{unit}]", color=MUTED, fontsize=12)
    ax.set_ylabel(f"y [{unit}]", color=MUTED, fontsize=12)


def make_body_artists(ax: plt.Axes, earth_size: float, moon_size: float | None) -> dict:
    """Estelas, puntos de los registros y marcadores de un panel."""
    artists = {
        "earth_trail": ax.plot([], [], color=EARTH_TRAIL, lw=1.2, alpha=0.9, zorder=3)[0],
        "earth_records": ax.plot([], [], ls="none", marker="o", ms=2.6,
                                 color=RECORD_COLOR, alpha=0.75, zorder=4)[0],
        "earth": ax.plot([], [], ls="none", marker="o", ms=earth_size, color=EARTH_COLOR,
                         mec=EARTH_EDGE, mew=0.8, zorder=6)[0],
    }
    if moon_size is not None:
        artists |= {
            "moon_trail": ax.plot([], [], color=MOON_TRAIL, lw=1.2, alpha=0.9, zorder=3)[0],
            "moon_records": ax.plot([], [], ls="none", marker="o", ms=2.6,
                                    color=RECORD_COLOR, alpha=0.75, zorder=4)[0],
            "moon": ax.plot([], [], ls="none", marker="o", ms=moon_size, color=MOON_COLOR,
                            mec="#ffffff", mew=0.5, zorder=6)[0],
        }
    # Se dibuja por encima del marcador de la Tierra: en la vista principal el
    # baricentro queda dentro del símbolo (ampliado) de la Tierra, igual que
    # queda dentro de la Tierra real.
    ax.plot([0.0], [0.0], ls="none", marker="o", ms=5, color=BARYCENTER_COLOR, zorder=7)
    return artists


def label(ax: plt.Axes, text: str, offset: tuple[float, float], color: str,
          xy: tuple[float, float] = (0.0, 0.0), size: float = 13):
    return ax.annotate(text, xy=xy, xytext=offset, textcoords="offset points",
                       color=color, fontsize=size, zorder=8)


def build_figure(p: Positions):
    plt.rcParams["font.family"] = "DejaVu Sans"
    fig = plt.figure(figsize=(WIDTH / DPI, HEIGHT / DPI), dpi=DPI, facecolor=BACKGROUND)

    # Dos paneles cuadrados del mismo tamaño en píxeles (734,4 x 734,4 px).
    side_h = 0.68
    side_w = side_h * HEIGHT / WIDTH
    ax_main = fig.add_axes([0.075, 0.165, side_w, side_h])
    ax_zoom = fig.add_axes([0.545, 0.165, side_w, side_h])

    style_panel(ax_main, MAIN_HALF_RANGE_KM, 100_000.0,
                FuncFormatter(lambda v, _: f"{v / 1e3:.0f}"), "10³ km")
    style_panel(ax_zoom, ZOOM_HALF_RANGE_KM, 2_000.0,
                FuncFormatter(lambda v, _: thousands(v)), "km")

    magnification = MAIN_HALF_RANGE_KM / ZOOM_HALF_RANGE_KM
    ax_main.set_title("Sistema completo — órbita lunar", color=TEXT, fontsize=15, loc="left",
                      pad=10)
    ax_zoom.set_title(f"Acercamiento al baricentro — órbita terrestre  (×{magnification:.0f})",
                      color=TEXT, fontsize=15, loc="left", pad=10)

    main = make_body_artists(ax_main, earth_size=12, moon_size=9)
    zoom = make_body_artists(ax_zoom, earth_size=18, moon_size=None)

    main["earth_label"] = label(ax_main, "Tierra", (10, 9), EARTH_EDGE)
    main["moon_label"] = label(ax_main, "Luna", (10, 8), MOON_COLOR)
    label(ax_main, "Baricentro (0, 0)", (10, -20), BARYCENTER_COLOR, size=12)
    zoom["earth_label"] = label(ax_zoom, "Tierra", (14, 11), EARTH_EDGE)
    label(ax_zoom, "Baricentro (0, 0)", (9, -19), BARYCENTER_COLOR, size=12)

    legend_handles = [
        Line2D([], [], ls="none", marker="o", ms=10, color=EARTH_COLOR, mec=EARTH_EDGE,
               label="Tierra"),
        Line2D([], [], ls="none", marker="o", ms=8, color=MOON_COLOR, label="Luna"),
        Line2D([], [], ls="none", marker="o", ms=5, color=BARYCENTER_COLOR,
               label="Baricentro (0, 0)"),
        Line2D([], [], color=EARTH_TRAIL, lw=1.2, label="Trayectoria de la Tierra"),
        Line2D([], [], color=MOON_TRAIL, lw=1.2, label="Trayectoria de la Luna"),
        Line2D([], [], ls="none", marker="o", ms=3, color=RECORD_COLOR,
               label="Registro diario de GRAVITAS"),
    ]
    # Fuera de los paneles, para que nunca oculte parte de una órbita.
    fig.legend(handles=legend_handles, loc="center", bbox_to_anchor=(0.5, 0.075),
               ncol=len(legend_handles), fontsize=11.5, frameon=False, labelcolor=TEXT,
               handlelength=1.8, columnspacing=2.2)

    # Encabezado.
    fig.text(0.075, 0.950, "GRAVITAS — Sistema Tierra-Luna", color=TEXT, fontsize=28,
             fontweight="bold", va="center")
    fig.text(0.075, 0.905, "Simulación gravitacional de N cuerpos", color=MUTED, fontsize=17,
             va="center")
    fig.text(0.327, 0.905, "·   Velocity Verlet  ·  Δt = 60 s  ·  posiciones relativas al "
             "baricentro", color=MUTED, fontsize=12, va="center")

    # Contador del tiempo simulado.
    fig.text(0.927, 0.966, "TIEMPO SIMULADO", color=MUTED, fontsize=12, ha="right",
             va="center")
    counter = fig.text(0.927, 0.929, "", color=TEXT, fontsize=30, ha="right", va="center",
                       family="DejaVu Sans Mono")
    status = fig.text(0.927, 0.893, "", color=MUTED, fontsize=12, ha="right", va="center")

    # Nota al pie que documenta qué está (y qué no) a escala, y la interpolación.
    fig.text(0.075, 0.038,
             "Posiciones: salida de GRAVITAS, un registro cada 24 h (puntos). Los fotogramas "
             "intermedios son interpolaciones lineales solo para visualización; no se "
             "recalcula ninguna órbita.",
             color=MUTED, fontsize=11.5, va="center")
    fig.text(0.075, 0.016,
             "Las distancias y ambos ejes están a escala. El tamaño de los cuerpos NO está a "
             "escala (el radio real de la Tierra, 6371 km, supera su órbita alrededor del "
             "baricentro).",
             color=MUTED, fontsize=11.5, va="center")

    axes = {"principal": ax_main, "de acercamiento": ax_zoom}
    return fig, axes, main, zoom, counter, status


# ==============================================================================
# Animación
# ==============================================================================

def make_update(p, times, earth_frames, moon_frames, main, zoom, counter, status):
    last_day = int(p.days[-1])
    motion_end = HOLD_START_FRAMES + last_day * FRAMES_PER_DAY

    def update(frame: int):
        t = times[frame]
        passed = int(np.floor(t))  # Registros ya alcanzados (días 0..passed).
        earth_now, moon_now = earth_frames[frame], moon_frames[frame]

        # Estela = puntos registrados hasta ahora + posición actual. Con
        # interpolación lineal, es exactamente el camino recorrido por los
        # marcadores.
        earth_path = np.vstack([p.earth[: passed + 1], earth_now])
        moon_path = np.vstack([p.moon[: passed + 1], moon_now])

        for panel in (main, zoom):
            panel["earth_trail"].set_data(earth_path[:, 0], earth_path[:, 1])
            panel["earth_records"].set_data(p.earth[: passed + 1, 0], p.earth[: passed + 1, 1])
            panel["earth"].set_data([earth_now[0]], [earth_now[1]])
            panel["earth_label"].xy = tuple(earth_now)
        main["moon_trail"].set_data(moon_path[:, 0], moon_path[:, 1])
        main["moon_records"].set_data(p.moon[: passed + 1, 0], p.moon[: passed + 1, 1])
        main["moon"].set_data([moon_now[0]], [moon_now[1]])
        main["moon_label"].xy = tuple(moon_now)

        counter.set_text(f"{decimal(t, 2, 6)} días")
        if frame < HOLD_START_FRAMES:
            status.set_text("Estado inicial  ·  registro GRAVITAS, día 0")
        elif frame > motion_end:
            status.set_text(f"Estado final  ·  registro GRAVITAS, día {last_day}")
        elif t == passed:
            status.set_text(f"Registro GRAVITAS, día {passed}")
        else:
            status.set_text(f"Interpolado entre los días {passed} y {passed + 1}")
        return []

    return update


def select_codec() -> tuple[str, list[str]]:
    """H.264 (libx264) si esta compilación de FFmpeg lo incluye; si no, MPEG-4 Part 2."""
    encoders = subprocess.run(["ffmpeg", "-hide_banner", "-encoders"],
                              capture_output=True, text=True).stdout
    if " libx264 " in encoders:
        return "libx264", ["-pix_fmt", "yuv420p", "-crf", "18", "-preset", "medium",
                           "-movflags", "+faststart"]
    print("AVISO: libx264 no está disponible; se usará MPEG-4 Part 2 (no es H.264).")
    return "mpeg4", ["-pix_fmt", "yuv420p", "-q:v", "2"]


def main() -> int:
    if shutil.which("ffmpeg") is None or not animation.writers.is_available("ffmpeg"):
        print("ERROR: no se encontró FFmpeg en el PATH; es necesario para exportar el MP4.")
        return 1

    positions = load_positions(DATA_FILE)
    validate_data(positions)

    earth_before, moon_before = positions.earth.copy(), positions.moon.copy()
    times = build_timeline(int(positions.days[-1]))
    earth_frames = interpolate(times, positions.days, positions.earth)
    moon_frames = interpolate(times, positions.days, positions.moon)
    validate_interpolation(positions, times, earth_frames, moon_frames,
                           earth_before, moon_before)

    fig, axes, main_artists, zoom_artists, counter, status = build_figure(positions)
    fig.canvas.draw()  # Resuelve la disposición para que las transformaciones sean definitivas.
    validate_scales(axes, positions)

    codec, extra_args = select_codec()
    update = make_update(positions, times, earth_frames, moon_frames,
                         main_artists, zoom_artists, counter, status)
    anim = animation.FuncAnimation(fig, update, frames=len(times), blit=False)

    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    print(f"Renderizando {len(times)} fotogramas ({decimal(len(times) / FPS, 2)} s a {FPS} fps, "
          f"{WIDTH}x{HEIGHT}, códec {codec}) ...")
    writer = animation.FFMpegWriter(fps=FPS, codec=codec, extra_args=extra_args)

    def progress(i: int, n: int) -> None:
        if i % (5 * FPS) == 0 or i == n - 1:
            print(f"  fotograma {i + 1:>5}/{n}")

    anim.save(str(OUTPUT_FILE), writer=writer, dpi=DPI, progress_callback=progress)
    plt.close(fig)

    if not OUTPUT_FILE.is_file() or OUTPUT_FILE.stat().st_size == 0:
        print(f"ERROR: la exportación falló; no se escribió {OUTPUT_FILE}.")
        return 1
    print(f"Video generado: {OUTPUT_FILE} ({decimal(OUTPUT_FILE.stat().st_size / 1e6, 1)} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
