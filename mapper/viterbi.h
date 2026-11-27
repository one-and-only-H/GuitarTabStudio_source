
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

    // Calcula el costo de pasar de una posición melódica a otra usando traste, cuerda y sustain
    inline double costoTransicionMelodica(
        const CandidatoMelodico& anterior,
        const CandidatoMelodico& actual,
        const ConfigMapper& config,
        int anclaPrev = -1,
        double inicioActual = 0.0,
        double cuerdaLibreEn = 0.0
    ) {
        // Si no se pasa ancla externa (ej. tests unitarios), usar el traste anterior
        if (anclaPrev < 0) {
            anclaPrev = anterior.posicion.traste;
        }

        const int maxTraste = std::max(1, config.traste_maximo);
        const int deltaCuerda = std::abs(anterior.posicion.cuerda - actual.posicion.cuerda);
        double cambioCuerda = static_cast<double>(deltaCuerda) / 5.0;

        // 1. Movimiento de traste con memoria de ancla (anclaPrev)
        double movimientoTraste = 0.0;
        if (actual.posicion.traste > 0) {
            const int refAncla = (anclaPrev > 0) ? anclaPrev : 1;
            const int deltaTraste = std::abs(refAncla - actual.posicion.traste);
            movimientoTraste = static_cast<double>(deltaTraste) / maxTraste;

            // Cruzar a cuerda adyacente dentro de la misma caja de dedos es casi gratis
            if (deltaCuerda == 1 && deltaTraste <= 2) {
                cambioCuerda *= 0.25;
            }
            // Desplazarse >= 2 trastes en la misma cuerda cuesta más que cruzar de cuerda en caja
            if (deltaCuerda == 0 && deltaTraste >= 2) {
                movimientoTraste *= 1.6;
            }
            // Penalización extra si excede la extensión cómoda de la mano
            // Penalización progresiva desde saltos >= max_hand_span (4 trastes)
            if (deltaTraste >= config.max_hand_span) {
                movimientoTraste +=
                    static_cast<double>(deltaTraste - config.max_hand_span + 1) /
                    std::max(1, config.max_hand_span);
            }
        } else if (actual.posicion.traste == 0 && anclaPrev >= 6) {
            // Permite usar cuerdas al aire en primera posición (trastes 1-5),
            // pero penaliza soltar al aire cuando la mano ya subió a zona media/alta (>= 6).
            movimientoTraste = static_cast<double>(anclaPrev) / maxTraste;
            if (anclaPrev >= config.max_hand_span) {
                movimientoTraste +=
                    static_cast<double>(anclaPrev - config.max_hand_span + 1) /
                    std::max(1, config.max_hand_span);
            }
        }

        // 2. Penalización solo si hay solapamiento real de sustain en la cuerda elegida
        constexpr double HOLGURA_SOLAPAMIENTO = 0.015; // 15 ms de tolerancia
        constexpr double PENALIZACION_CUERDA_OCUPADA = 0.30;
        double penalizacionSustain = 0.0;

        // 2. Penalización por cuerda ocupada / resonancia



        if (inicioActual < cuerdaLibreEn &&
            cuerdaLibreEn > 0.0) {

            const int deltaTrasteMismaCuerda =
                    std::abs(
                    anterior.posicion.traste -
                    actual.posicion.traste
                );

            const bool esPosibleLigado =
                (deltaCuerda == 0) &&
                (deltaTrasteMismaCuerda >= 1 &&
                deltaTrasteMismaCuerda <= 3);

            if (!esPosibleLigado) {
                penalizacionSustain =
                PENALIZACION_CUERDA_OCUPADA;
            }
}

        // 2. Penalización por resonancia/sustain activo en la cuerda elegida
   
        

        return
            config.w_transicion_traste * movimientoTraste +
            config.w_transicion_cuerda * cambioCuerda +
            penalizacionSustain;
    }

    // --------------------------------------------------------
    // Calcula el centro de un voicing
    // --------------------------------------------------------

    // Obtiene el traste medio del voicing ignorando las cuerdas al aire
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


    // Obtiene la cuerda media ocupada por las posiciones de un voicing
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

    // Calcula el costo de cambiar entre dos posturas armónicas comparando notas y centros
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

    // Ejecuta el algoritmo general de Viterbi usando costos locales y de transición dados
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

// Resuelve una secuencia melódica conservando el ancla de la mano y el estado de cada cuerda
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
        double costo_acumulado =
            std::numeric_limits<double>::infinity();

        int indice_candidato = -1;
        int indice_anterior = -1;

        int ancla_traste = 0;

        std::array<double, 6> cuerda_libre_en{};
    };

    constexpr std::size_t ANCHO_BEAM = 96;
    constexpr std::size_t MAX_ESTADOS_POR_CANDIDATO = 4;

    constexpr double VENTANA_RESONANCIA = 0.075;
    constexpr double PENALIZACION_CUERDA_OCUPADA = 0.45;

    const std::size_t N = candidatos.size();

    std::vector<std::vector<NodoMelodico>> tabla(N);

    // ========================================================
    // 1. INICIALIZACIÓN
    // ========================================================

    tabla[0].reserve(candidatos[0].size());

    const double inicioNota0 =
        (!notasOriginales.empty())
            ? notasOriginales[0].timestamp
            : 0.0;

    const double finNota0 =
        (!notasOriginales.empty())
            ? notasOriginales[0].timestamp +
              notasOriginales[0].duracion
            : 0.0;

    for (std::size_t j = 0; j < candidatos[0].size(); ++j) {

        NodoMelodico nodo;

        nodo.costo_acumulado = candidatos[0][j].costo;
        nodo.indice_candidato = static_cast<int>(j);
        nodo.indice_anterior = -1;

        // Una cuerda abierta no establece ancla.
        nodo.ancla_traste =
            candidatos[0][j].posicion.traste;

        const int idxCuerda =
            candidatos[0][j].posicion.cuerda - 1;

        if (idxCuerda >= 0 && idxCuerda < 6) {

            nodo.cuerda_libre_en[idxCuerda] =
                finNota0 + VENTANA_RESONANCIA;
        }

        tabla[0].push_back(nodo);
    }

    // ========================================================
    // 2. RECURRENCIA CON MEMORIA FÍSICA
    // ========================================================

    for (std::size_t i = 1; i < N; ++i) {

        const double inicioActual =
            (i < notasOriginales.size())
                ? notasOriginales[i].timestamp
                : 0.0;

        const double finActual =
            (i < notasOriginales.size())
                ? notasOriginales[i].timestamp +
                  notasOriginales[i].duracion
                : inicioActual;

        std::vector<NodoMelodico> nuevosEstados;

        nuevosEstados.reserve(
            tabla[i - 1].size() * candidatos[i].size()
        );

        // Cada estado previo puede evolucionar hacia
        // cualquier candidato del instante actual.

        for (std::size_t k = 0;
             k < tabla[i - 1].size();
             ++k) {

            const NodoMelodico& previo = tabla[i - 1][k];

            const CandidatoMelodico& notaPrev =
                candidatos[i - 1][previo.indice_candidato];

            for (std::size_t j = 0;
                 j < candidatos[i].size();
                 ++j) {

                const CandidatoMelodico& actual =
                    candidatos[i][j];

                const int idxCuerdaActual =
                    actual.posicion.cuerda - 1;

                const double libreEn =
                    (idxCuerdaActual >= 0 &&
                     idxCuerdaActual < 6)
                        ? previo.cuerda_libre_en[idxCuerdaActual]
                        : 0.0;

                const double costoTransicion =
                    detalleViterbi::costoTransicionMelodica(
                        notaPrev,
                        actual,
                        config,
                        previo.ancla_traste,
                        inicioActual,
                        libreEn
                    );

                NodoMelodico nuevo;

                nuevo.costo_acumulado =
                    previo.costo_acumulado +
                    costoTransicion +
                    actual.costo;

                nuevo.indice_candidato =
                    static_cast<int>(j);

                nuevo.indice_anterior =
                    static_cast<int>(k);

                // Las cuerdas abiertas NO desplazan el ancla.
                nuevo.ancla_traste =
                    (actual.posicion.traste > 0)
                        ? actual.posicion.traste
                        : previo.ancla_traste;

                nuevo.cuerda_libre_en =
                    previo.cuerda_libre_en;

                if (idxCuerdaActual >= 0 &&
                    idxCuerdaActual < 6) {

                    nuevo.cuerda_libre_en[idxCuerdaActual] =
                        std::max(
                            nuevo.cuerda_libre_en[idxCuerdaActual],
                            finActual + VENTANA_RESONANCIA
                        );
                }

                nuevosEstados.push_back(nuevo);
            }
        }

        // ====================================================
        // 3. PRUNING DEL BEAM
        // ====================================================

        std::sort(
            nuevosEstados.begin(),
            nuevosEstados.end(),
            [](const NodoMelodico& a,
               const NodoMelodico& b) {

                return a.costo_acumulado <
                       b.costo_acumulado;
            }
        );

        tabla[i].reserve(ANCHO_BEAM);

        std::vector<std::size_t> cantidadPorCandidato(
            candidatos[i].size(),
            0
        );

        for (const NodoMelodico& estado : nuevosEstados) {

            const std::size_t idx =
                static_cast<std::size_t>(
                    estado.indice_candidato
                );

            // Evitar que un único candidato absorba
            // todo el beam con diferentes historiales.

            if (cantidadPorCandidato[idx] >=
                MAX_ESTADOS_POR_CANDIDATO) {

                continue;
            }

            tabla[i].push_back(estado);
            cantidadPorCandidato[idx]++;

            if (tabla[i].size() >= ANCHO_BEAM) {
                break;
            }
        }

        // Protección: si el pruning dejase el beam vacío,
        // conservar el mejor estado disponible.

        if (tabla[i].empty() && !nuevosEstados.empty()) {
            tabla[i].push_back(nuevosEstados.front());
        }
    }

    // ========================================================
    // 4. MEJOR ESTADO FINAL
    // ========================================================

    if (tabla.back().empty()) {
        return camino;
    }

    int mejorFinal = 0;

    for (std::size_t j = 1;
         j < tabla.back().size();
         ++j) {

        if (tabla.back()[j].costo_acumulado <
            tabla.back()[mejorFinal].costo_acumulado) {

            mejorFinal = static_cast<int>(j);
        }
    }

    // ========================================================
    // 5. RECONSTRUCCIÓN
    // ========================================================

    camino.resize(N);

    int indiceEstado = mejorFinal;

    for (int i = static_cast<int>(N) - 1;
         i >= 0;
         --i) {

        if (indiceEstado < 0 ||
            indiceEstado >=
                static_cast<int>(tabla[i].size())) {

            return {};
        }

        camino[i] =
            tabla[i][indiceEstado].indice_candidato;

        indiceEstado =
            tabla[i][indiceEstado].indice_anterior;
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

// Llama al Viterbi general con el costo local y el costo de transición armónico
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
