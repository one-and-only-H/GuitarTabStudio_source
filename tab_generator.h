#ifndef TAB_GENERATOR_H
#define TAB_GENERATOR_H

#include "mapper/mapper.h"

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace TabGenerator {

// ============================================================
// CONFIGURACIÓN DEL GENERADOR DE TABLATURA
// ============================================================
struct ConfigTab {
    // Escala temporal: cuántos guiones representan 1 segundo en pasajes activos
    double guiones_por_segundo = 8.0;

    // Mínimo de guiones de separación entre eventos consecutivos
    int min_guiones_evento = 2;

    // Máximo de guiones para un silencio individual antes de truncar espacio visual
    int max_guiones_individual = 24;

    // Umbral (en segundos) a partir del cual un silencio TOTAL en las 6 cuerdas
    // se comprime en un bloque multiplicador (ej. "|  x5  |")
    double umbral_silencio_total = 1.5;

    // Ancho máximo de caracteres por bloque/sistema antes de hacer salto de línea
    int ancho_maximo_linea = 100;

    // Nombres de las 6 cuerdas (1 = aguda e, 6 = grave E)
    std::array<std::string, 6> nombres_cuerdas = {"e", "B", "G", "D", "A", "E"};

    // Mostrar fila superior con marcas de tiempo (en segundos)
    bool mostrar_tiempo = true;

    // Mostrar fila de nombres de acordes cuando exista evento armónico
    bool mostrar_acordes = true;
};

// ============================================================
// ESTRUCTURAS INTERMEDIAS DE RENDERIZADO
// ============================================================

// Estado visual de una cuerda en una columna específica
struct CeldaCuerda {
    int traste = -1;           // -1 = sin nota (guiones), >= 0 = número de traste
    bool es_sostenida = false; // true = se imprime entre paréntesis "(traste)"
};

// Representa una columna vertical alineada en las 8 filas
struct ColumnaTab {
    double timestamp = 0.0;
    double duracion = 0.0;

    bool es_silencio_total = false; // true cuando las 6 cuerdas están inactivas por >= umbral
    double segundos_silencio = 0.0; // Duración exacta para mostrar en "| xN |"

    std::string etiqueta_tiempo;    // Ej: "0.0s", "1.5s"
    std::string etiqueta_acorde;    // Ej: "Em", "Cadd9", "" si no hay acorde

    std::array<CeldaCuerda, 6> cuerdas; // Índice 0 = cuerda 1 (e), índice 5 = cuerda 6 (E)
    int ancho_visual = 0;               // Ancho exacto en caracteres de toda la columna
};

// ============================================================
// 1. UTILIDADES DE FORMATO PRIMITIVAS
// ============================================================

inline std::string formatearTiempo(double segundos) {
    char buffer[32];
    if (std::abs(segundos - std::round(segundos)) < 0.05) {
        std::snprintf(buffer, sizeof(buffer), "%.0fs", std::round(segundos));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1fs", segundos);
    }
    return std::string(buffer);
}

inline std::string formatearMultiplicador(double segundos) {
    char buffer[32];
    if (std::abs(segundos - std::round(segundos)) < 0.05) {
        std::snprintf(buffer, sizeof(buffer), "x%.0f", std::round(segundos));
    } else {
        std::snprintf(buffer, sizeof(buffer), "x%.1f", segundos);
    }
    return std::string(buffer);
}

inline std::string formatearCeldaCuerda(const CeldaCuerda& celda) {
    if (celda.traste < 0) {
        return "";
    }
    if (celda.es_sostenida) {
        return "(" + std::to_string(celda.traste) + ")";
    }
    return std::to_string(celda.traste);
}

inline int calcularAnchoGuiones(double deltaTiempo, const ConfigTab& config) {
    if (deltaTiempo <= 0.0) {
        return config.min_guiones_evento;
    }
    int guiones = static_cast<int>(std::round(deltaTiempo * config.guiones_por_segundo));
    return std::clamp(guiones, config.min_guiones_evento, config.max_guiones_individual);
}

inline std::string padDerecha(const std::string& texto, int ancho, char relleno) {
    if (static_cast<int>(texto.size()) >= ancho) {
        return texto;
    }
    return texto + std::string(static_cast<std::size_t>(ancho - texto.size()), relleno);
}

inline std::string centrarEnBarra(const std::string& texto, int anchoInterior, char relleno) {
    if (static_cast<int>(texto.size()) >= anchoInterior) {
        return "|" + texto + "|";
    }
    int totalRelleno = anchoInterior - static_cast<int>(texto.size());
    int izq = totalRelleno / 2;
    int der = totalRelleno - izq;
    return "|" +
           std::string(static_cast<std::size_t>(izq), relleno) +
           texto +
           std::string(static_cast<std::size_t>(der), relleno) +
           "|";
}

// ============================================================
// 2. CONSTRUCCIÓN DE COLUMNAS (ATAQUES, SOSTENIDOS Y SILENCIOS)
// ============================================================

inline std::vector<ColumnaTab> construirColumnas(
    const Mapper::ResultadoMapeo& resultado,
    const ConfigTab& config = {}
) {
    std::vector<ColumnaTab> columnas;
    if (resultado.eventos.empty()) {
        return columnas;
    }

    // Copiar y ordenar cronológicamente los eventos por seguridad
    std::vector<Mapper::TabEvent> eventos = resultado.eventos;
    std::sort(
        eventos.begin(),
        eventos.end(),
        [](const Mapper::TabEvent& a, const Mapper::TabEvent& b) {
            return a.timestamp < b.timestamp;
        }
    );

    struct EstadoCuerdaActiva {
        int traste = -1;
        double fin = 0.0;
    };
    std::array<EstadoCuerdaActiva, 6> estadoCuerdas{};

    constexpr double EPS = 1e-3;
    double cursorTiempo = 0.0;

    // Helper para procesar un hueco temporal [inicioHueco, finHueco) antes de un ataque
    auto insertarHuecoSiAplica = [&](double inicioHueco, double finHueco) {
        double delta = finHueco - inicioHueco;
        if (delta <= EPS) {
            return;
        }

        // Revisar si alguna cuerda sigue sosteniendo una nota durante el hueco
        bool hayNotaSostenida = false;
        for (int c = 0; c < 6; ++c) {
            if (estadoCuerdas[c].traste >= 0 && estadoCuerdas[c].fin > inicioHueco + EPS) {
                hayNotaSostenida = true;
                break;
            }
        }

        // Caso A: Silencio TOTAL en las 6 cuerdas y dura >= umbral_silencio_total
        if (!hayNotaSostenida && delta >= config.umbral_silencio_total) {
            ColumnaTab colSilencio;
            colSilencio.timestamp = inicioHueco;
            colSilencio.duracion = delta;
            colSilencio.es_silencio_total = true;
            colSilencio.segundos_silencio = delta;
            colSilencio.etiqueta_tiempo = config.mostrar_tiempo ? formatearTiempo(inicioHueco) : "";
            colSilencio.etiqueta_acorde = "";

            std::string mult = " " + formatearMultiplicador(delta) + " ";
            int anchoInterior = std::max({
                static_cast<int>(mult.size()),
                static_cast<int>(colSilencio.etiqueta_tiempo.size()),
                5
            });
            // El ancho visual incluye las dos barras '|' laterales
            colSilencio.ancho_visual = anchoInterior + 2;
            columnas.push_back(std::move(colSilencio));
            return;
        }

        // Caso B: Hay nota(s) sostenida(s) mientras otras cuerdas callan,
        // o es un silencio corto (< umbral_silencio_total).
        if (hayNotaSostenida) {
            ColumnaTab colSostenido;
            colSostenido.timestamp = inicioHueco;
            colSostenido.duracion = delta;
            colSostenido.es_silencio_total = false;
            colSostenido.etiqueta_tiempo = "";
            colSostenido.etiqueta_acorde = "";

            int anchoTextoMax = 0;
            for (int c = 0; c < 6; ++c) {
                if (estadoCuerdas[c].traste >= 0 && estadoCuerdas[c].fin > inicioHueco + EPS) {
                    colSostenido.cuerdas[c].traste = estadoCuerdas[c].traste;
                    colSostenido.cuerdas[c].es_sostenida = true;
                    std::string txt = formatearCeldaCuerda(colSostenido.cuerdas[c]);
                    anchoTextoMax = std::max(anchoTextoMax, static_cast<int>(txt.size()));
                }
            }

            int anchoTiempo = calcularAnchoGuiones(delta, config);
            colSostenido.ancho_visual = std::max(anchoTextoMax + 1, anchoTiempo);
            columnas.push_back(std::move(colSostenido));
        } else if (!columnas.empty()) {
            // Silencio total corto (< umbral): simplemente ensanchamos la columna anterior
            // con guiones adicionales proporcionales al hueco
            int guionesExtra = calcularAnchoGuiones(delta, config);
            columnas.back().ancho_visual += guionesExtra;
        }
    };

    for (std::size_t idx = 0; idx < eventos.size(); ++idx) {
        const auto& ev = eventos[idx];

        // 1. Si hay un intervalo vacío entre cursorTiempo y el ataque actual, resolverlo
        if (ev.timestamp > cursorTiempo + EPS) {
            insertarHuecoSiAplica(cursorTiempo, ev.timestamp);
        }

        // 2. Construir la columna del evento atacado
        ColumnaTab col;
        col.timestamp = ev.timestamp;
        col.duracion = ev.duracion;
        col.es_silencio_total = false;

        if (config.mostrar_tiempo) {
            col.etiqueta_tiempo = formatearTiempo(ev.timestamp);
        }
        if (config.mostrar_acordes && ev.acorde.has_value() && !ev.acorde->nombreAcorde.empty()) {
            col.etiqueta_acorde = ev.acorde->nombreAcorde;
        }

        // Primero heredar notas sostenidas que aún no hayan expirado en este instante
        for (int c = 0; c < 6; ++c) {
            if (estadoCuerdas[c].traste >= 0 && estadoCuerdas[c].fin > ev.timestamp + EPS) {
                col.cuerdas[c].traste = estadoCuerdas[c].traste;
                col.cuerdas[c].es_sostenida = true;
            } else {
                estadoCuerdas[c].traste = -1;
                estadoCuerdas[c].fin = 0.0;
            }
        }

        // Sobrescribir con las notas que atacan en este evento
        double maxFinEvento = ev.timestamp + ev.duracion;
        for (const auto& nota : ev.notas) {
            int idxCuerda = nota.cuerda - 1;
            if (idxCuerda < 0 || idxCuerda >= 6) {
                continue;
            }
            col.cuerdas[idxCuerda].traste = nota.traste;
            col.cuerdas[idxCuerda].es_sostenida = false;

            double finNota = nota.timestamp + nota.duracion;
            estadoCuerdas[idxCuerda].traste = nota.traste;
            estadoCuerdas[idxCuerda].fin = finNota;
            maxFinEvento = std::max(maxFinEvento, finNota);
        }

        // Calcular el ancho visual exacto de la columna para alinear las 8 filas
        int anchoContenido = 1;
        if (!col.etiqueta_tiempo.empty()) {
            anchoContenido = std::max(anchoContenido, static_cast<int>(col.etiqueta_tiempo.size()) + 1);
        }
        if (!col.etiqueta_acorde.empty()) {
            anchoContenido = std::max(anchoContenido, static_cast<int>(col.etiqueta_acorde.size()) + 1);
        }
        for (int c = 0; c < 6; ++c) {
            std::string txt = formatearCeldaCuerda(col.cuerdas[c]);
            if (!txt.empty()) {
                anchoContenido = std::max(anchoContenido, static_cast<int>(txt.size()) + 1);
            }
        }

        // Calcular cuánto tiempo transcurre hasta el siguiente ataque o hasta el fin de este evento
        double siguienteTiempo = (idx + 1 < eventos.size())
            ? eventos[idx + 1].timestamp
            : maxFinEvento;
        double deltaPaso = std::max(0.0, std::min(siguienteTiempo - ev.timestamp, ev.duracion));
        int anchoPorTiempo = calcularAnchoGuiones(deltaPaso, config);

        col.ancho_visual = std::max(anchoContenido, anchoPorTiempo);
        columnas.push_back(std::move(col));

        // Avanzar el cursor hasta el final de la duración base del evento
        // (si una nota individual dura más que el acorde, estadoCuerdas[c].fin la sostendrá como "(traste)")
        double finBaseEvento = ev.timestamp + std::max(0.0, ev.duracion);
        if (idx + 1 < eventos.size()) {
            cursorTiempo = std::min(finBaseEvento, eventos[idx + 1].timestamp);
        } else {
            cursorTiempo = finBaseEvento;
        }
    }

    // 3. Si al terminar todos los eventos alguna nota sigue resonando más allá del último acorde,
    //    añadir una columna final de sostenido "(traste)"
    double maxColaSostenida = cursorTiempo;
    for (int c = 0; c < 6; ++c) {
        maxColaSostenida = std::max(maxColaSostenida, estadoCuerdas[c].fin);
    }
    if (maxColaSostenida > cursorTiempo + EPS) {
        insertarHuecoSiAplica(cursorTiempo, maxColaSostenida);
    }

    return columnas;
}

// ============================================================
// 3. RENDERIZADO DE SISTEMA Y GENERACIÓN COMPLETA
// ============================================================

inline std::string renderizarSistema(
    const std::vector<ColumnaTab>& columnas,
    std::size_t inicio,
    std::size_t fin,
    const ConfigTab& config
) {
    // Prefijo izquierdo alineado (ej. "Tiempo | ", "Acorde | ", "e      | ")
    const int anchoPrefijo = 7;

    std::string lineaTiempo = padDerecha("Tiempo", anchoPrefijo, ' ') + "|";
    std::string lineaAcorde = padDerecha("Acorde", anchoPrefijo, ' ') + "|";
    std::array<std::string, 6> lineasCuerdas;
    for (int c = 0; c < 6; ++c) {
        lineasCuerdas[c] = padDerecha(config.nombres_cuerdas[c], anchoPrefijo, ' ') + "|";
    }

    for (std::size_t i = inicio; i < fin; ++i) {
        const auto& col = columnas[i];

        if (col.es_silencio_total) {
            int anchoInterior = std::max(1, col.ancho_visual - 2);
            std::string mult = " " + formatearMultiplicador(col.segundos_silencio) + " ";

            lineaTiempo += centrarEnBarra(col.etiqueta_tiempo, anchoInterior, ' ');
            lineaAcorde += centrarEnBarra("", anchoInterior, ' ');

            // El multiplicador " x5 " va centrado en las cuerdas centrales (G / D)
            // y el resto lleva barras '|' alineadas con espacios
            for (int c = 0; c < 6; ++c) {
                if (c == 2) {
                    lineasCuerdas[c] += centrarEnBarra(mult, anchoInterior, '-');
                } else {
                    lineasCuerdas[c] += centrarEnBarra("", anchoInterior, '-');
                }
            }
        } else {
            lineaTiempo += padDerecha(col.etiqueta_tiempo, col.ancho_visual, ' ');
            lineaAcorde += padDerecha(col.etiqueta_acorde, col.ancho_visual, ' ');

            for (int c = 0; c < 6; ++c) {
                std::string txt = formatearCeldaCuerda(col.cuerdas[c]);
                lineasCuerdas[c] += padDerecha(txt, col.ancho_visual, '-');
            }
        }
    }

    // Cierre derecho del sistema
    lineaTiempo += "|";
    lineaAcorde += "|";
    for (int c = 0; c < 6; ++c) {
        lineasCuerdas[c] += "|";
    }

    std::ostringstream out;
    if (config.mostrar_tiempo) {
        out << lineaTiempo << '\n';
    }
    if (config.mostrar_acordes) {
        out << lineaAcorde << '\n';
    }
    for (int c = 0; c < 6; ++c) {
        out << lineasCuerdas[c] << '\n';
    }

    return out.str();
}

inline std::string generarTablatura(
    const Mapper::ResultadoMapeo& resultado,
    const ConfigTab& config = {}
) {
    auto columnas = construirColumnas(resultado, config);
    if (columnas.empty()) {
        return "Tiempo |\nAcorde |\ne      |\nB      |\nG      |\nD      |\nA      |\nE      |\n";
    }

    std::ostringstream salida;
    const int anchoPrefijoTotal = 9; // "Tiempo |" + "|" final
    const int limiteContenido = std::max(20, config.ancho_maximo_linea - anchoPrefijoTotal);

    std::size_t inicio = 0;
    while (inicio < columnas.size()) {
        std::size_t fin = inicio;
        int anchoAcumulado = 0;

        while (fin < columnas.size()) {
            int anchoCol = columnas[fin].ancho_visual;
            if (fin > inicio && (anchoAcumulado + anchoCol) > limiteContenido) {
                break;
            }
            anchoAcumulado += anchoCol;
            ++fin;
        }

        salida << renderizarSistema(columnas, inicio, fin, config);
        if (fin < columnas.size()) {
            salida << '\n';
        }
        inicio = fin;
    }

    return salida.str();
}

inline bool exportarTablaturaTxt(
    const Mapper::ResultadoMapeo& resultado,
    const std::string& rutaArchivo,
    const ConfigTab& config = {}
) {
    std::ofstream archivo(rutaArchivo, std::ios::out | std::ios::trunc);
    if (!archivo.is_open()) {
        return false;
    }

    archivo << generarTablatura(resultado, config);
    return archivo.good();
}
bool procesarYExportar(
    const Mapper::ResultadoMapeo& resultado,
    const std::string& rutaArchivoSalida,
    std::string* salidaTextoOut = nullptr,
    const ConfigTab& config = {}
);

bool procesarDesdeNotas(
    const std::vector<Nota>& notasRaw,
    const std::string& rutaArchivoSalida,
    std::string* salidaTextoOut = nullptr,
    const ConfigTab& config = {}
);


} // namespace TabGenerator

#endif // TAB_GENERATOR_H