
#ifndef VITERBI_H
#define VITERBI_H

#include "tipos_mapper.h"
#include "analizador_patrones.h"

#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <array>
#include <limits>

// ============================================================
// COSTOS DE TRANSICIÓN
// ============================================================

namespace detalleViterbi {

    // --------------------------------------------------------
    // Distancia normalizada entre dos posiciones melódicas
    // --------------------------------------------------------

    inline double costoTransicionMelodica(
        const CandidatoMelodico& anterior,
        const CandidatoMelodico& actual,
        const ConfigMapper& config
    ) {
        // Si alguna de las dos notas es al aire, el costo de movimiento es 0.
        if (anterior.posicion.traste == 0 || actual.posicion.traste == 0) {
        // Solo cobramos el cambio de cuerda, si lo hubo (porque cruzar de cuerda
        // sí requiere mover la mano, aunque sea mínimamente).
            double cambioCuerda = std::abs(anterior.posicion.cuerda - actual.posicion.cuerda) / 5.0;
            return config.w_transicion_cuerda * cambioCuerda;
        }
        int maxTraste = std::max(1, config.traste_maximo);

        double movimientoTraste =
            static_cast<double>(
                std::abs(
                    anterior.posicion.traste -
                    actual.posicion.traste
                )
            ) / maxTraste;

        double cambioCuerda =
            static_cast<double>(
                std::abs(
                    anterior.posicion.cuerda -
                    actual.posicion.cuerda
                )
            ) / 5.0;

        return
            config.w_transicion_traste * movimientoTraste +
            config.w_transicion_cuerda * cambioCuerda;
    }


    // --------------------------------------------------------
    // Calcula el centro de un voicing
    // --------------------------------------------------------

    inline double centroTrastes(
    const CandidatoVoicing& candidato
) {
    if (candidato.posiciones.empty()) {
        return 0.0;
    }

    double suma = 0.0;
    int cuentan = 0;

    for (const auto& posicion : candidato.posiciones) {
        // Ignoramos las cuerdas al aire porque no atan la mano a ninguna posición.
        if (posicion.traste > 0) {
            suma += posicion.traste;
            cuentan++;
        }
    }

    // Si el acorde es 100% al aire (ej. Em con todas las cuerdas al aire),
    // el centro real de la mano es 0.
    if (cuentan == 0) {
        return 0.0;
    }

    return suma / cuentan;
}


    inline double centroCuerdas(
        const CandidatoVoicing& candidato
    ) {
        if (candidato.posiciones.empty()) {
            return 0.0;
        }

        double suma = 0.0;

        for (const auto& posicion : candidato.posiciones) {
            suma += posicion.cuerda;
        }

        return suma / candidato.posiciones.size();
    }


    // --------------------------------------------------------
    // Distancia entre dos voicings
    // --------------------------------------------------------

    inline double costoTransicionArmonica(
        const CandidatoVoicing& anterior,
        const CandidatoVoicing& actual,
        const ConfigMapper& config
    ) {
        if (anterior.posiciones.empty() ||
            actual.posiciones.empty()) {
            return 1.0;
        }

        int maxTraste = std::max(1, config.traste_maximo);

        // ----------------------------------------------------
        // A. Movimiento de notas compartidas
        // ----------------------------------------------------

        double sumaMovimiento = 0.0;
        int notasCompartidas = 0;

        for (const auto& posAnterior : anterior.posiciones) {

    for (const auto& posActual : actual.posiciones) {

        if (posAnterior.pitch == posActual.pitch) {

            // Si alguna de las dos posiciones es al aire,
            // no hay movimiento de traste (solo se levanta/apoya el dedo).
            double movimientoTraste = 0.0;
            if (posAnterior.traste != 0 && posActual.traste != 0) {
                movimientoTraste =
                    static_cast<double>(
                        std::abs(
                            posAnterior.traste -
                            posActual.traste
                        )
                    ) / maxTraste;
            }

            // El cambio de cuerda sí se cobra siempre, 
            // porque cruzar de cuerda implica mover la mano o los dedos.
            double cambioCuerda =
                static_cast<double>(std::abs(posAnterior.cuerda - posActual.cuerda) ) / 5.0;

                    sumaMovimiento +=
                       (movimientoTraste + cambioCuerda) / 2.0;

                    notasCompartidas++;

                    break;
                }
            }
        }

        double C_voicing = 0.0;

        if (notasCompartidas > 0) {
            C_voicing =
                sumaMovimiento / notasCompartidas;
        }


        // ----------------------------------------------------
        // B. Movimiento del centro de la mano
        // ----------------------------------------------------

        double movimientoCentroTrastes =
            std::abs(
                centroTrastes(anterior) -
                centroTrastes(actual)
            ) / maxTraste;

        double movimientoCentroCuerdas =
            std::abs(
                centroCuerdas(anterior) -
                centroCuerdas(actual)
            ) / 5.0;

        double C_centro =
            (movimientoCentroTrastes +
             movimientoCentroCuerdas) / 2.0;


        // Si no hay notas compartidas, usamos el centro
        // como referencia para mantener continuidad.

        if (notasCompartidas == 0) {
            C_voicing = C_centro;
        }

        return
            config.w_transicion_voicing * C_voicing +
            config.w_transicion_centro * C_centro;
    }


    // ========================================================
    // ALGORITMO GENERAL DE VITERBI
    // ========================================================

    // candidatos[i] contiene los candidatos disponibles
    // en el instante i.
    //
    // obtenerCosto devuelve el costo local de un candidato.
    //
    // obtenerTransicion devuelve el costo de pasar
    // de un candidato anterior a uno actual.
    //
    // Devuelve el índice del candidato elegido en cada instante.

    template <typename Candidato, typename ObtenerCosto,
              typename ObtenerTransicion>

    inline std::vector<int> ejecutarViterbi(
        const std::vector<std::vector<Candidato>>& candidatos,
        ObtenerCosto obtenerCosto,
        ObtenerTransicion obtenerTransicion
    ) {
        std::vector<int> camino;

        if (candidatos.empty()) {
            return camino;
        }

        // Si algún instante no tiene candidatos,
        // no es posible construir un camino completo.

        for (const auto& instante : candidatos) {
            if (instante.empty()) {
                return camino;
            }
        }

        // Tabla de estados:
        // una fila por instante y una columna por candidato.

        std::vector<std::vector<EstadoViterbi>> tabla(
            candidatos.size()
        );

        // ----------------------------------------------------
        // 1. Inicialización
        // ----------------------------------------------------

        tabla[0].resize(candidatos[0].size());

        for (size_t j = 0; j < candidatos[0].size(); j++) {

            tabla[0][j].costo_acumulado =
                obtenerCosto(candidatos[0][j]);

            tabla[0][j].indice_anterior = -1;
        }


        // ----------------------------------------------------
        // 2. Recurrencia
        // ----------------------------------------------------

        for (size_t i = 1; i < candidatos.size(); i++) {

            tabla[i].resize(candidatos[i].size());

            for (size_t j = 0; j < candidatos[i].size(); j++) {

                double mejorCosto =
                    std::numeric_limits<double>::infinity();

                int mejorAnterior = -1;

                // Probar todos los candidatos del instante anterior.

                for (size_t k = 0;
                     k < candidatos[i - 1].size();
                     k++) {

                    double costoTransicion =
                        obtenerTransicion(
                            candidatos[i - 1][k],
                            candidatos[i][j]
                        );

                    double costoTotal =
                        tabla[i - 1][k].costo_acumulado +
                        costoTransicion +
                        obtenerCosto(candidatos[i][j]);

                    if (costoTotal < mejorCosto) {

                        mejorCosto = costoTotal;

                        mejorAnterior =
                            static_cast<int>(k);
                    }
                }

                tabla[i][j].costo_acumulado =
                    mejorCosto;

                tabla[i][j].indice_anterior =
                    mejorAnterior;
            }
        }


        // ----------------------------------------------------
        // 3. Encontrar el mejor estado final
        // ----------------------------------------------------

        int mejorFinal = 0;

        double menorCosto =
            tabla.back()[0].costo_acumulado;

        for (size_t j = 1; j < tabla.back().size(); j++) {

            if (tabla.back()[j].costo_acumulado < menorCosto) {

                menorCosto =
                    tabla.back()[j].costo_acumulado;

                mejorFinal = static_cast<int>(j);
            }
        }


        // ----------------------------------------------------
        // 4. Reconstruir el camino óptimo
        // ----------------------------------------------------

        camino.resize(candidatos.size());

        int indiceActual = mejorFinal;

        for (int i = static_cast<int>(candidatos.size()) - 1;
             i >= 0;
             i--) {

            camino[i] = indiceActual;

            indiceActual =
                tabla[i][indiceActual].indice_anterior;
        }

        return camino;
    }

}


// ============================================================
// VITERBI MELÓDICO
// ============================================================

// Cada elemento de candidatos representa un instante
// de una misma secuencia melódica.
//
// Devuelve el índice del candidato elegido en cada instante.

inline std::vector<int> resolverViterbiMelodico(
    const std::vector<std::vector<CandidatoMelodico>>& candidatos,
    const std::vector<Nota>& notasOriginales,
    const ConfigMapper& config
) {
    std::vector<int> camino;

    if (candidatos.empty()) {
        return camino;
    }

    for (const auto& instante : candidatos) {
        if (instante.empty()) {
            return camino;
        }
    }

    struct NodoMelodico {
        double costo_acumulado = std::numeric_limits<double>::infinity();
        int indice_anterior = -1;
        int ancla_traste = 0; // Último traste > 0 pisado por la mano
        std::array<double, 6> cuerda_libre_en{}; // Timestamp en que queda libre cada cuerda (0..5)
    };

    const int maxTraste = std::max(1, config.traste_maximo);
    const std::size_t N = candidatos.size();
    constexpr double HOLGURA_LEGATO = 0.025;      // 25 ms de margen para hammer-ons / pull-offs
    constexpr double PENALIZACION_CUERDA_OCUPADA = 0.45;

    std::vector<std::vector<NodoMelodico>> tabla(N);

    // 1. Inicialización
    tabla[0].resize(candidatos[0].size());
    const double finNota0 = (0 < notasOriginales.size())
        ? (notasOriginales[0].timestamp + notasOriginales[0].duracion)
        : 0.0;

    for (std::size_t j = 0; j < candidatos[0].size(); ++j) {
        tabla[0][j].costo_acumulado = candidatos[0][j].costo;
        tabla[0][j].indice_anterior = -1;
        tabla[0][j].ancla_traste = candidatos[0][j].posicion.traste;

        int idxCuerda = candidatos[0][j].posicion.cuerda - 1;
        if (idxCuerda >= 0 && idxCuerda < 6) {
            tabla[0][j].cuerda_libre_en[idxCuerda] = finNota0;
        }
    }

    // 2. Recurrencia con memoria de mano y estado de ocupación de cuerdas
    for (std::size_t i = 1; i < N; ++i) {
        tabla[i].resize(candidatos[i].size());

        const double inicioActual = (i < notasOriginales.size())
            ? notasOriginales[i].timestamp
            : 0.0;
        const double finActual = (i < notasOriginales.size())
            ? (notasOriginales[i].timestamp + notasOriginales[i].duracion)
            : 0.0;

        for (std::size_t j = 0; j < candidatos[i].size(); ++j) {
            const auto& actual = candidatos[i][j];
            const int idxCuerdaActual = actual.posicion.cuerda - 1;

            for (std::size_t k = 0; k < candidatos[i - 1].size(); ++k) {
                const auto& prev = candidatos[i - 1][k];
                const int anclaPrev = tabla[i - 1][k].ancla_traste;

                double cambioCuerda =
                    static_cast<double>(
                        std::abs(prev.posicion.cuerda - actual.posicion.cuerda)
                    ) / 5.0;

                double movimientoTraste = 0.0;
                if (actual.posicion.traste > 0 && anclaPrev > 0) {
                    int deltaTraste = std::abs(anclaPrev - actual.posicion.traste);
                    movimientoTraste =
                        static_cast<double>(deltaTraste) / maxTraste;

                    if (deltaTraste > config.max_hand_span) {
                        movimientoTraste +=
                            static_cast<double>(deltaTraste - config.max_hand_span) /
                            std::max(1, config.max_hand_span);
                    }
                } else if (actual.posicion.traste == 0 && anclaPrev >= 5) {
                    movimientoTraste =
                        static_cast<double>(anclaPrev) / maxTraste;
                }

                // Penalización si la cuerda elegida aún tiene una nota sonando
                // más allá de la holgura de legato (25 ms)
                double penalizacionSustain = 0.0;
                if (idxCuerdaActual >= 0 && idxCuerdaActual < 6) {
                    double libreEn = tabla[i - 1][k].cuerda_libre_en[idxCuerdaActual];
                    if (inicioActual < (libreEn - HOLGURA_LEGATO)) {
                        penalizacionSustain = PENALIZACION_CUERDA_OCUPADA;
                    }
                }

                double costoTransicion =
                    config.w_transicion_traste * movimientoTraste +
                    config.w_transicion_cuerda * cambioCuerda +
                    penalizacionSustain;

                double costoTotal =
                    tabla[i - 1][k].costo_acumulado +
                    costoTransicion +
                    actual.costo;

                if (costoTotal < tabla[i][j].costo_acumulado) {
                    tabla[i][j].costo_acumulado = costoTotal;
                    tabla[i][j].indice_anterior = static_cast<int>(k);
                    tabla[i][j].ancla_traste =
                        (actual.posicion.traste > 0)
                            ? actual.posicion.traste
                            : anclaPrev;

                    tabla[i][j].cuerda_libre_en = tabla[i - 1][k].cuerda_libre_en;
                    if (idxCuerdaActual >= 0 && idxCuerdaActual < 6) {
                        tabla[i][j].cuerda_libre_en[idxCuerdaActual] =
                            std::max(tabla[i][j].cuerda_libre_en[idxCuerdaActual], finActual);
                    }
                }
            }
        }
    }

    // 3. Mejor estado final
    int mejorFinal = 0;
    double menorCosto = tabla.back()[0].costo_acumulado;

    for (std::size_t j = 1; j < tabla.back().size(); ++j) {
        if (tabla.back()[j].costo_acumulado < menorCosto) {
            menorCosto = tabla.back()[j].costo_acumulado;
            mejorFinal = static_cast<int>(j);
        }
    }

    // 4. Reconstrucción del camino
    camino.resize(N);
    int indiceActual = mejorFinal;

    for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
        camino[i] = indiceActual;
        indiceActual = tabla[i][indiceActual].indice_anterior;
    }

    return camino;
}


// ============================================================
// VITERBI ARMÓNICO
// ============================================================

// Cada elemento de candidatos representa un evento armónico.
// Cada candidato contiene un voicing completo.
//
// Devuelve el índice del voicing elegido en cada evento.

inline std::vector<int> resolverViterbiArmonico(
    const std::vector<std::vector<CandidatoVoicing>>& candidatos,
    const ConfigMapper& config
) {
    auto obtenerCosto =
        [](const CandidatoVoicing& candidato) {
            return candidato.costo;
        };

    auto obtenerTransicion =
        [&config](
            const CandidatoVoicing& anterior,
            const CandidatoVoicing& actual
        ) {
            return detalleViterbi::costoTransicionArmonica(
                anterior,
                actual,
                config
            );
        };

    return detalleViterbi::ejecutarViterbi(
        candidatos,
        obtenerCosto,
        obtenerTransicion
    );
}

#endif
