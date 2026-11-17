
#ifndef ACORDES_H
#define ACORDES_H

#include "tipos.h"

#include <string>
#include <vector>

// Índices utilizados por EventoArmonico::quality_index.
namespace CalidadAcorde {
    constexpr int MAYOR = 0;
    constexpr int MENOR = 1;
    constexpr int DISMINUIDO = 2;
    constexpr int AUMENTADO = 3;
    constexpr int SUS2 = 4;
    constexpr int SUS4 = 5;
    constexpr int SEPTIMA = 6;
    constexpr int MAYOR_SEPTIMA = 7;
    constexpr int MENOR_SEPTIMA = 8;
    constexpr int SEMIDISMINUIDO = 9;
    constexpr int POWER_CHORD = 10;
}

// Resultado del análisis tonal.
struct Tonalidad {
    int tonica_pitch_class = -1;
    bool mayor = true;

    std::string nombre;

    double correlacion = 0.0;
};

// Agrupa notas según su proximidad temporal.
std::vector<EventoArmonico> agruparNotas(
    const std::vector<Nota>& notas,
    double tolerancia = 0.05
    
);

// Reconoce los acordes presentes en los eventos.
void reconocerAcordes(
    std::vector<EventoArmonico>& eventos,
    double umbral_confianza = 0.60
);

// Estima la tonalidad general de una secuencia de eventos.
Tonalidad detectarTonalidad(
    const std::vector<EventoArmonico>& eventos
);

// Transpone una nota individual.
Nota transponerNota(
    const Nota& nota,
    int semitonos
);

// Transpone todas las notas y la información armónica de un evento.
EventoArmonico transponerAcorde(
    const EventoArmonico& evento,
    int semitonos
);

#endif
