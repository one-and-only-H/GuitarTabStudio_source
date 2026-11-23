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
// CONFIGURACIÓN DEL GENERADOR DE TABLATURA POR COMPASES
// ============================================================
struct ConfigTab {
    // Parámetros rítmicos del compás
    double bpm = 120.0;
    int numerador_compas = 4;
    int denominador_compas = 4;

    // Ancho interior fijo (en caracteres) de cada compás activo
    // Garantiza velocidad visual constante en el desplazamiento horizontal
    int ancho_compas = 24;

    // Cantidad mínima de compases completos en silencio para comprimir en "| - xN - |"
    int min_compases_silencio = 2;

    // Ancho máximo de caracteres por bloque/sistema antes de hacer salto de línea
    int ancho_maximo_linea = 100;

    // Nombres de las 6 cuerdas (1 = aguda e, 6 = grave E)
    std::array<std::string, 6> nombres_cuerdas = {"e", "B", "G", "D", "A", "E"};

    // Mostrar fila superior con número de compás
    bool mostrar_tiempo = true;

    // Mostrar fila de nombres de acordes cuando exista evento armónico
    bool mostrar_acordes = true;
};

// ============================================================
// ESTRUCTURAS INTERMEDIAS DE RENDERIZADO
// ============================================================

struct CeldaCuerda {
    int traste = -1;           // -1 = sin nota (guiones), >= 0 = número de traste
    bool es_sostenida = false; // true = se imprime entre paréntesis "(traste)"
};

struct ColumnaTab {
    double timestamp = 0.0;
    double duracion = 0.0;

    bool es_silencio_total = false; // true cuando representa compases completos en silencio
    bool cierra_compas = false;     // true en la última subdivisión del compás para dibujar '|'
    int numero_compas = 1;          // Número de compás (1-based)
    int compases_silencio = 0;      // Cantidad entera (floor) de compases vacíos comprimidos

    std::string etiqueta_tiempo;    // Ej: "1", "2", "15"
    std::string etiqueta_acorde;    // Ej: "Em", "Cadd9", "" si no hay acorde

    std::array<CeldaCuerda, 6> cuerdas; // Índice 0 = cuerda 1 (e), índice 5 = cuerda 6 (E)
    int ancho_visual = 0;               // Ancho exacto en caracteres (incluye '|' si cierra_compas)
};

// ============================================================
// 1. UTILIDADES DE FORMATO Y CÁLCULO DE COMPÁS
// ============================================================

inline double calcularDuracionCompas(const ConfigTab& config) {
    const double bpmValido = std::max(1.0, config.bpm);
    const int numValido = std::max(1, config.numerador_compas);
    const int denValido = std::max(1, config.denominador_compas);

    const double duracionNegra = 60.0 / bpmValido;
    const double duracionPulso = duracionNegra * (4.0 / static_cast<double>(denValido));
    return std::max(0.05, duracionPulso * static_cast<double>(numValido));
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

inline std::string padDerecha(const std::string& texto, int ancho, char relleno) {
    if (static_cast<int>(texto.size()) >= ancho) {
        return texto;
    }
    return texto + std::string(static_cast<std::size_t>(ancho - texto.size()), relleno);
}

inline std::string centrarTexto(const std::string& texto, int anchoInterior, char relleno) {
    if (static_cast<int>(texto.size()) >= anchoInterior) {
        return texto.substr(0, static_cast<std::size_t>(anchoInterior));
    }
    const int totalRelleno = anchoInterior - static_cast<int>(texto.size());
    const int izq = totalRelleno / 2;
    const int der = totalRelleno - izq;
    return std::string(static_cast<std::size_t>(izq), relleno) +
           texto +
           std::string(static_cast<std::size_t>(der), relleno);
}

// ============================================================
// 2. CONSTRUCCIÓN DE COLUMNAS POR COMPASES
// ============================================================

inline std::vector<ColumnaTab> construirColumnas(
    const Mapper::ResultadoMapeo& resultado,
    const ConfigTab& config = {}
) {
    std::vector<ColumnaTab> columnas;
    if (resultado.eventos.empty()) {
        return columnas;
    }

    std::vector<Mapper::TabEvent> eventos = resultado.eventos;
    std::sort(
        eventos.begin(),
        eventos.end(),
        [](const Mapper::TabEvent& a, const Mapper::TabEvent& b) {
            return a.timestamp < b.timestamp;
        }
    );

    constexpr double EPS = 1e-3;
    const double duracionCompas = calcularDuracionCompas(config);

    // Determinar el instante final real de la última nota
    double tiempoFinCancion = 0.0;
    for (const auto& ev : eventos) {
        tiempoFinCancion = std::max(tiempoFinCancion, ev.timestamp + ev.duracion);
        for (const auto& nota : ev.notas) {
            tiempoFinCancion = std::max(tiempoFinCancion, nota.timestamp + nota.duracion);
        }
    }

    const int totalCompases = std::max(
        1,
        static_cast<int>(std::ceil((tiempoFinCancion - EPS) / duracionCompas))
    );

    struct EstadoCuerdaActiva {
        int traste = -1;
        double fin = 0.0;
    };
    std::array<EstadoCuerdaActiva, 6> estadoCuerdas{};

    // Helper para comprobar si un compás m tiene ataques o notas sostenidas
    auto compasTieneActividad = [&](int m, const std::array<EstadoCuerdaActiva, 6>& estadoActual) -> bool {
        const double t0 = m * duracionCompas;
        const double t1 = (m + 1) * duracionCompas;

        for (int c = 0; c < 6; ++c) {
            if (estadoActual[c].traste >= 0 && estadoActual[c].fin > t0 + EPS) {
                return true;
            }
        }
        for (const auto& ev : eventos) {
            if (ev.timestamp >= t0 - EPS && ev.timestamp < t1 - EPS) {
                return true;
            }
        }
        return false;
    };

    std::size_t cursorEvento = 0;
    while (cursorEvento < eventos.size() && eventos[cursorEvento].timestamp < -EPS) {
        ++cursorEvento;
    }

    int m = 0;
    while (m < totalCompases) {
        // Caso 1: Compás completamente vacío -> contar compases enteros (floor) en silencio
        if (!compasTieneActividad(m, estadoCuerdas)) {
            int mFinSilencio = m + 1;
            while (mFinSilencio < totalCompases && !compasTieneActividad(mFinSilencio, estadoCuerdas)) {
                ++mFinSilencio;
            }

            const double tInicioSilencio = m * duracionCompas;
            const double tFinSilencio = mFinSilencio * duracionCompas;
            const int compasesVacios = static_cast<int>(
                std::floor((tFinSilencio - tInicioSilencio + EPS) / duracionCompas)
            );

            if (compasesVacios >= std::max(1, config.min_compases_silencio)) {
                ColumnaTab colSilencio;
                colSilencio.timestamp = tInicioSilencio;
                colSilencio.duracion = compasesVacios * duracionCompas;
                colSilencio.es_silencio_total = true;
                colSilencio.cierra_compas = true;
                colSilencio.numero_compas = m + 1;
                colSilencio.compases_silencio = compasesVacios;
                colSilencio.etiqueta_tiempo = config.mostrar_tiempo ? std::to_string(m + 1) : "";

                const std::string textoMult = " x" + std::to_string(compasesVacios) + " ";
                const int anchoInterior = std::max({
                    static_cast<int>(textoMult.size()) + 2,
                    static_cast<int>(colSilencio.etiqueta_tiempo.size()) + 2,
                    8
                });
                colSilencio.ancho_visual = anchoInterior + 1; // +1 por la barra '|' de cierre
                columnas.push_back(std::move(colSilencio));

                m = mFinSilencio;
                continue;
            }
            // Si hay menos compases vacíos que min_compases_silencio (ej. 1 solo compás vacío),
            // se renderiza como un compás normal vacío del mismo ancho para mantener fluidez
        }

        // Caso 2: Compás activo (o compás individual vacío no comprimido)
        const double tInicioCompas = m * duracionCompas;
        const double tFinCompas = (m + 1) * duracionCompas;

        // Recolectar los índices de eventos cuyo ataque cae dentro de [tInicioCompas, tFinCompas)
        std::vector<std::size_t> indicesEnCompas;
        while (cursorEvento < eventos.size() &&
               eventos[cursorEvento].timestamp < tFinCompas - EPS) {
            indicesEnCompas.push_back(cursorEvento);
            ++cursorEvento;
        }

        // Construir los cortes temporales dentro del compás [tInicioCompas, tFinCompas]
        std::vector<double> cortes;
        cortes.push_back(tInicioCompas);

        for (std::size_t idxEv : indicesEnCompas) {
            const double tEv = std::clamp(eventos[idxEv].timestamp, tInicioCompas, tFinCompas);
            if (tEv - cortes.back() > EPS && tFinCompas - tEv > EPS) {
                cortes.push_back(tEv);
            }
        }

        // Añadir cortes donde termina alguna nota sostenida dentro del compás
        std::array<EstadoCuerdaActiva, 6> simCuerdas = estadoCuerdas;
        for (int c = 0; c < 6; ++c) {
            if (simCuerdas[c].traste >= 0 &&
                simCuerdas[c].fin > tInicioCompas + EPS &&
                simCuerdas[c].fin < tFinCompas - EPS) {
                cortes.push_back(simCuerdas[c].fin);
            }
        }
        for (std::size_t idxEv : indicesEnCompas) {
            for (const auto& nota : eventos[idxEv].notas) {
                const double finN = nota.timestamp + nota.duracion;
                if (finN > tInicioCompas + EPS && finN < tFinCompas - EPS) {
                    cortes.push_back(finN);
                }
            }
        }

        cortes.push_back(tFinCompas);
        std::sort(cortes.begin(), cortes.end());

        std::vector<double> cortesUnicos;
        cortesUnicos.reserve(cortes.size());
        for (double t : cortes) {
            if (cortesUnicos.empty() || (t - cortesUnicos.back()) > EPS) {
                cortesUnicos.push_back(t);
            }
        }
        if (cortesUnicos.size() < 2) {
            cortesUnicos = {tInicioCompas, tFinCompas};
        }

        const std::size_t numSlices = cortesUnicos.size() - 1;
        std::vector<ColumnaTab> slicesCompas(numSlices);
        std::vector<int> anchosMinimos(numSlices, 1);

        std::size_t ptrEv = 0;
        for (std::size_t s = 0; s < numSlices; ++s) {
            const double t0 = cortesUnicos[s];
            const double t1 = cortesUnicos[s + 1];

            ColumnaTab& col = slicesCompas[s];
            col.timestamp = t0;
            col.duracion = t1 - t0;
            col.es_silencio_total = false;
            col.cierra_compas = (s + 1 == numSlices);
            col.numero_compas = m + 1;

            if (s == 0 && config.mostrar_tiempo) {
                col.etiqueta_tiempo = std::to_string(m + 1);
            }

            // Limpiar cuerdas que ya expiraron antes de t0
            for (int c = 0; c < 6; ++c) {
                if (estadoCuerdas[c].traste >= 0 && estadoCuerdas[c].fin <= t0 + EPS) {
                    estadoCuerdas[c].traste = -1;
                    estadoCuerdas[c].fin = 0.0;
                }
            }

            // Heredar notas sostenidas vigentes en t0
            for (int c = 0; c < 6; ++c) {
                if (estadoCuerdas[c].traste >= 0 && estadoCuerdas[c].fin > t0 + EPS) {
                    col.cuerdas[c].traste = estadoCuerdas[c].traste;
                    col.cuerdas[c].es_sostenida = true;
                }
            }

            // Aplicar cualquier evento que ataque en t0
            while (ptrEv < indicesEnCompas.size()) {
                const auto& ev = eventos[indicesEnCompas[ptrEv]];
                const double tEv = std::clamp(ev.timestamp, tInicioCompas, tFinCompas);
                if (tEv > t0 + EPS) {
                    break;
                }

                if (config.mostrar_acordes && ev.acorde.has_value() && !ev.acorde->nombreAcorde.empty()) {
                    col.etiqueta_acorde = ev.acorde->nombreAcorde;
                }

                for (const auto& nota : ev.notas) {
                    const int idxCuerda = nota.cuerda - 1;
                    if (idxCuerda < 0 || idxCuerda >= 6) {
                        continue;
                    }
                    col.cuerdas[idxCuerda].traste = nota.traste;
                    col.cuerdas[idxCuerda].es_sostenida = false;

                    estadoCuerdas[idxCuerda].traste = nota.traste;
                    estadoCuerdas[idxCuerda].fin = nota.timestamp + nota.duracion;
                }
                ++ptrEv;
            }

            // Calcular el ancho mínimo necesario para que no se trunque ningún texto
            int minW = 1;
            if (!col.etiqueta_tiempo.empty()) {
                minW = std::max(minW, static_cast<int>(col.etiqueta_tiempo.size()) + 1);
            }
            if (!col.etiqueta_acorde.empty()) {
                minW = std::max(minW, static_cast<int>(col.etiqueta_acorde.size()) + 1);
            }
            for (int c = 0; c < 6; ++c) {
                const std::string txt = formatearCeldaCuerda(col.cuerdas[c]);
                if (!txt.empty()) {
                    minW = std::max(minW, static_cast<int>(txt.size()) + 1);
                }
            }
            anchosMinimos[s] = minW;
        }

        // Distribuir el ancho fijo del compás proporcionalmente al tiempo de cada subdivisión
        int sumaMinimos = 0;
        for (int w : anchosMinimos) sumaMinimos += w;
        const int anchoInteriorObjetivo = std::max(config.ancho_compas, sumaMinimos);

        std::vector<int> anchosAsignados = anchosMinimos;
        int restante = anchoInteriorObjetivo - sumaMinimos;

        if (restante > 0) {
            std::vector<double> fracciones(numSlices, 0.0);
            int repartido = 0;
            for (std::size_t s = 0; s < numSlices; ++s) {
                const double idealExtra = (slicesCompas[s].duracion / duracionCompas) * restante;
                const int enteroExtra = static_cast<int>(std::floor(idealExtra));
                anchosAsignados[s] += enteroExtra;
                fracciones[s] = idealExtra - static_cast<double>(enteroExtra);
                repartido += enteroExtra;
            }

            // Repartir el sobrante de redondeo a las subdivisiones con mayor residuo fraccional
            int sobrante = restante - repartido;
            while (sobrante > 0) {
                std::size_t mejorIdx = 0;
                for (std::size_t s = 1; s < numSlices; ++s) {
                    if (fracciones[s] > fracciones[mejorIdx]) {
                        mejorIdx = s;
                    }
                }
                anchosAsignados[mejorIdx] += 1;
                fracciones[mejorIdx] = -1.0;
                --sobrante;
            }
        }

        for (std::size_t s = 0; s < numSlices; ++s) {
            slicesCompas[s].ancho_visual = anchosAsignados[s] + (slicesCompas[s].cierra_compas ? 1 : 0);
            columnas.push_back(std::move(slicesCompas[s]));
        }

        ++m;
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
    // Prefijo izquierdo de 7 caracteres ASCII + '|' inicial = 8 columnas exactas
    const int anchoPrefijo = 7;

    std::string lineaCompas = padDerecha("Compas", anchoPrefijo, ' ') + "|";
    std::string lineaAcorde = padDerecha("Acorde", anchoPrefijo, ' ') + "|";
    std::array<std::string, 6> lineasCuerdas;
    for (int c = 0; c < 6; ++c) {
        lineasCuerdas[c] = padDerecha(config.nombres_cuerdas[c], anchoPrefijo, ' ') + "|";
    }

    for (std::size_t i = inicio; i < fin; ++i) {
        const auto& col = columnas[i];

        if (col.es_silencio_total) {
            const int anchoInterior = std::max(1, col.ancho_visual - 1);
            const std::string mult = " x" + std::to_string(col.compases_silencio) + " ";

            lineaCompas += centrarTexto(col.etiqueta_tiempo, anchoInterior, ' ') + "|";
            lineaAcorde += std::string(static_cast<std::size_t>(anchoInterior), ' ') + "|";

            for (int c = 0; c < 6; ++c) {
                if (c == 2) {
                    lineasCuerdas[c] += centrarTexto(mult, anchoInterior, '-') + "|";
                } else {
                    lineasCuerdas[c] += std::string(static_cast<std::size_t>(anchoInterior), '-') + "|";
                }
            }
        } else {
            const int anchoInterior = std::max(1, col.ancho_visual - (col.cierra_compas ? 1 : 0));
            const std::string barra = col.cierra_compas ? "|" : "";

            lineaCompas += padDerecha(col.etiqueta_tiempo, anchoInterior, ' ') + barra;
            lineaAcorde += padDerecha(col.etiqueta_acorde, anchoInterior, ' ') + barra;

            for (int c = 0; c < 6; ++c) {
                const std::string txt = formatearCeldaCuerda(col.cuerdas[c]);
                lineasCuerdas[c] += padDerecha(txt, anchoInterior, '-') + barra;
            }
        }
    }

    std::ostringstream out;
    if (config.mostrar_tiempo) {
        out << lineaCompas << '\n';
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
        return "Compas |\nAcorde |\ne      |\nB      |\nG      |\nD      |\nA      |\nE      |\n";
    }

    std::ostringstream salida;
    const int anchoPrefijoTotal = 8; // "Compas |"
    const int limiteContenido = std::max(24, config.ancho_maximo_linea - anchoPrefijoTotal);

    std::size_t inicio = 0;
    while (inicio < columnas.size()) {
        std::size_t fin = inicio;
        int anchoAcumulado = 0;

        // Cortar líneas únicamente en límites completos de compás (cierra_compas == true)
        while (fin < columnas.size()) {
            std::size_t finCompas = fin;
            int anchoCompasActual = 0;
            while (finCompas < columnas.size()) {
                anchoCompasActual += columnas[finCompas].ancho_visual;
                const bool esCierre = columnas[finCompas].cierra_compas;
                ++finCompas;
                if (esCierre) {
                    break;
                }
            }

            if (fin > inicio && (anchoAcumulado + anchoCompasActual) > limiteContenido) {
                break;
            }
            anchoAcumulado += anchoCompasActual;
            fin = finCompas;
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