#ifndef TIPOS_H
#define TIPOS_H

#include <string>
#include <vector>

// Clasificación de una nota dentro de la música.
enum class Role {
    MELODIA,
    ACORDE,
    BAJO
};

// Técnica musical para tocar una nota
enum class Technique {
    None,
    Hammer,
    Pull,
    Slide,
    Bend,
    Vibrato,
    Unknown
};

// Representación general de una nota musical
struct Nota {
    std::string nota;
    int octava = 4;
    int pitch = -1;

    double duracion = 0.0;
    double timestamp = 0.0;

    int velocidad = 0;

    std::string tecnica;
    Technique tech = Technique::None;

    Role role = Role::ACORDE;

    float parametro = 0.0f;
};

// Conjunto de notas pertenecientes a un evento armónico (osea, al mismo tiempo salen las notas)
struct EventoArmonico {
    double timestamp = 0.0;
    double duracion = 0.0;

    std::vector<Nota> notas;

    std::string nombreAcorde;

    int root_pitch_class = -1;
    int quality_index = -1;
    int bass_pitch_class = -1;

    bool es_simultaneo = false;
};

// Ubicación de una nota en el diapasón.
struct PosicionGuitarra {
    int cuerda = 0;  // 1 = más aguda, 6 = más grave.
    int traste = 0;  // 0 = cuerda al aire.
    int pitch = -1;  // Pitch MIDI de la posición.
};

/*pitch class, osea, qué nota es según la escala cromática, 
entre 0 y 11. (C=0, C#=1, D=2, D#=3, E=4, F=5, F#=6, G=7, G#=8, A=9, A#=10, B=11)*/
inline int pitchClass(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

#endif
