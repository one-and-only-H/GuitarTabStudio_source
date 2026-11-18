#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <thread>
#include "tipos.h"

#pragma comment(lib, "winmm.lib")

// ============================================================
// CONSTANTES
// ============================================================
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr float MAX_PCM_VALUE = 32767.0f;
constexpr int NUM_HARMONICS = 7;
constexpr double PI = 3.14159265358979323846;

// Overdrive
constexpr float OD_DIAL_MIN = 0.235f;
constexpr float OD_DIAL_MAX = 2.5f;
constexpr float OD_BOOST = 2.5f;

// Limitador (solo para casos extremos o efectos)
constexpr float LIMITADOR_UMBRAL = 0.8f;

// EQ (ganancia de cada banda: 0.1 = -20dB, 1.0 = neutro, 2.0 = +6dB)
constexpr float EQ_GAIN_MIN = 0.1f;
constexpr float EQ_GAIN_MAX = 2.0f;
constexpr float EQ_GAIN_NEUTRO = 1.0f;

// Chorus
constexpr float CHORUS_DELAY_BASE = 0.015f;  // 15ms
constexpr float CHORUS_DELAY_MOD = 0.005f;   // ±5ms de modulación
constexpr int CHORUS_BUFFER_SIZE = 4096;      // ~93ms a 44.1kHz

// Tamaño del bloque de audio (en muestras)
constexpr int BLOQUE_MUESTRAS = 2048;         // ~46ms a 44.1kHz

// ============================================================
// STRUCTS
// ============================================================

enum class TipoPastilla { SINGLE_COIL, HUMBUCKER };
enum class TipoAtaque { LET_RING, MUTED };
enum class EstadoADSR { ATTACK, DECAY, SUSTAIN, RELEASE, OFF };

struct PerfilArmonico {
    float pesos[NUM_HARMONICS];
    float tasasDecaimiento[NUM_HARMONICS];
    float ataques[NUM_HARMONICS];
    float sustentos[NUM_HARMONICS];
    float releases[NUM_HARMONICS];
};

struct EnvolventeADSR {
    EstadoADSR estado = EstadoADSR::OFF;
    float valorActual = 0.0f;
    float tiempoEnEstado = 0.0f;
};

struct NotaSonando {
    int pitchOriginal;
    float duracionTotal;
    float tiempoTranscurrido;
    float timestampInicio;
    float volumenVelocity; // 0.0 a 1.0 (derivado de la velocidad MIDI)
    
    float frecuencias[NUM_HARMONICS];
    float fases[NUM_HARMONICS];
    EnvolventeADSR envolventes[NUM_HARMONICS];
    PerfilArmonico perfil;
};

// --- EQ (Biquad) ---
struct BiquadCoef {
    float b0, b1, b2, a1, a2;
};

struct BiquadEstado {
    float x1 = 0.0f, x2 = 0.0f;
    float y1 = 0.0f, y2 = 0.0f;
};

struct EQCoefs {
    BiquadCoef bass;
    BiquadCoef mid;
    BiquadCoef treble;
};

struct EQEstados {
    BiquadEstado bass;
    BiquadEstado mid;
    BiquadEstado treble;
};

// ============================================================
// FUNCIONES PRIMITIVAS (inline)
// ============================================================

// 1. MIDI a Frecuencia (La4 = 69 = 440 Hz)
inline float obtenerFrecuencia(int pitch) {
    return 440.0f * std::pow(2.0f, (pitch - 69) / 12.0f);
}

// 2. Armar los armónicos de una frecuencia fundamental
inline std::array<float, NUM_HARMONICS> dividirEnArmonicos(float frecuenciaFundamental) {
    std::array<float, NUM_HARMONICS> armonicos;
    for (int i = 0; i < NUM_HARMONICS; i++) {
        armonicos[i] = frecuenciaFundamental * (i + 1);
    }
    return armonicos;
}

// 3. Generar la onda de un armónico (actualiza la fase por referencia)
inline float generarOndaArmonico(float frecuencia, float& fase, float sampleRate) {
    float valor = std::sin(fase);
    float incrementoFase = (2.0f * static_cast<float>(PI) * frecuencia) / sampleRate;
    fase += incrementoFase;
    if (fase >= 2.0f * static_cast<float>(PI)) {
        fase -= 2.0f * static_cast<float>(PI);
    }
    return valor;
}

// 4. Generar los pre-samples de los 7 armónicos para UN sample
inline void generarPreSamples(
    const float frecuencias[NUM_HARMONICS],
    float fases[NUM_HARMONICS],
    float presamples[NUM_HARMONICS],
    float sampleRate
) {
    for (int i = 0; i < NUM_HARMONICS; i++) {
        presamples[i] = generarOndaArmonico(frecuencias[i], fases[i], sampleRate);
    }
}

// 5. Obtener el perfil combinado (pastilla + ataque)
inline PerfilArmonico obtenerPerfil(TipoPastilla pastilla, TipoAtaque ataque) {
    PerfilArmonico perfil{};
    
    if (pastilla == TipoPastilla::SINGLE_COIL) {
        float p[] = {1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
        float d[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        std::copy(p, p + NUM_HARMONICS, perfil.pesos);
        std::copy(d, d + NUM_HARMONICS, perfil.tasasDecaimiento);
    } else {
        float p[] = {1.0f, 0.9f, 0.6f, 0.3f, 0.2f, 0.1f, 0.05f};
        float d[] = {1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};
        std::copy(p, p + NUM_HARMONICS, perfil.pesos);
        std::copy(d, d + NUM_HARMONICS, perfil.tasasDecaimiento);
    }
    
    if (ataque == TipoAtaque::LET_RING) {
        float a[] = {0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f};
        float s[] = {0.80f, 0.70f, 0.60f, 0.50f, 0.40f, 0.30f, 0.20f};
        float r[] = {0.50f, 0.40f, 0.30f, 0.25f, 0.20f, 0.15f, 0.10f};
        std::copy(a, a + NUM_HARMONICS, perfil.ataques);
        std::copy(s, s + NUM_HARMONICS, perfil.sustentos);
        std::copy(r, r + NUM_HARMONICS, perfil.releases);
    } else {
        float a[] = {0.003f, 0.003f, 0.003f, 0.003f, 0.003f, 0.003f, 0.003f};
        float s[] = {0.10f, 0.08f, 0.05f, 0.03f, 0.02f, 0.01f, 0.00f};
        float r[] = {0.05f, 0.04f, 0.03f, 0.02f, 0.01f, 0.01f, 0.01f};
        std::copy(a, a + NUM_HARMONICS, perfil.ataques);
        std::copy(s, s + NUM_HARMONICS, perfil.sustentos);
        std::copy(r, r + NUM_HARMONICS, perfil.releases);
    }
    
    return perfil;
}

// 6. Avanzar un solo ADSR una muestra
inline float avanzarADSR(
    EnvolventeADSR& env,
    float tasaDecaimiento,
    float tiempoAtaque,
    float nivelSustain,
    float tiempoRelease,
    float duracionTotal,
    float tiempoTranscurrido,
    float sampleRate
) {
    float dt = 1.0f / sampleRate;
    env.tiempoEnEstado += dt;

    switch (env.estado) {
        case EstadoADSR::ATTACK:
            env.valorActual = env.tiempoEnEstado / tiempoAtaque;
            if (env.valorActual >= 1.0f) {
                env.valorActual = 1.0f;
                env.estado = EstadoADSR::DECAY;
                env.tiempoEnEstado = 0.0f;
            }
            break;
        case EstadoADSR::DECAY:
            env.valorActual = 1.0f - ((1.0f - nivelSustain) * (env.tiempoEnEstado / (0.5f * tasaDecaimiento)));
            if (env.valorActual <= nivelSustain) {
                env.valorActual = nivelSustain;
                env.estado = EstadoADSR::SUSTAIN;
                env.tiempoEnEstado = 0.0f;
            }
            break;
        case EstadoADSR::SUSTAIN:
            env.valorActual = nivelSustain;
            if (tiempoTranscurrido >= duracionTotal) {
                env.estado = EstadoADSR::RELEASE;
                env.tiempoEnEstado = 0.0f;
            }
            break;
        case EstadoADSR::RELEASE:
            env.valorActual = nivelSustain * (1.0f - (env.tiempoEnEstado / tiempoRelease));
            if (env.valorActual <= 0.0f) {
                env.valorActual = 0.0f;
                env.estado = EstadoADSR::OFF;
            }
            break;
        case EstadoADSR::OFF:
            env.valorActual = 0.0f;
            break;
    }
    return env.valorActual;
}

// 7. Aplicar la envolvente a los 7 pre-samples
inline void aplicarEnvolventes(
    float presamples[NUM_HARMONICS],
    EnvolventeADSR envolventes[NUM_HARMONICS],
    const PerfilArmonico& perfil,
    float duracionTotal,
    float tiempoTranscurrido,
    float sampleRate
) {
    for (int i = 0; i < NUM_HARMONICS; i++) {
        float valorEnv = avanzarADSR(
            envolventes[i],
            perfil.tasasDecaimiento[i],
            perfil.ataques[i],
            perfil.sustentos[i],
            perfil.releases[i],
            duracionTotal,
            tiempoTranscurrido,
            sampleRate
        );
        presamples[i] *= valorEnv;
    }
}

// 8. Mezclar los 7 armónicos en un solo sample
inline float mezclarArmonicos(const float presamples[NUM_HARMONICS], const float pesos[NUM_HARMONICS]) {
    float suma = 0.0f;
    for (int i = 0; i < NUM_HARMONICS; i++) {
        suma += presamples[i] * pesos[i];
    }
    return suma / static_cast<float>(NUM_HARMONICS);
}

// 9. Normalizar dividiendo entre sqrt(N) para evitar saltos de volumen
inline float normalizar(float suma, int N) {
    if (N < 1) N = 1;
    return suma / std::sqrt(static_cast<float>(N));
}

// 10. Aplicar un filtro biquad (un solo polo del EQ)
inline float aplicarBiquad(float x, BiquadEstado& s, const BiquadCoef& c) {
    float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
    s.x2 = s.x1; s.x1 = x;
    s.y2 = s.y1; s.y1 = y;
    return y;
}

// 11. Aplicar el EQ de 3 bandas
inline float aplicarEQ(float muestra, EQEstados& estados, const EQCoefs& coefs) {
    float y = aplicarBiquad(muestra, estados.bass, coefs.bass);
    y = aplicarBiquad(y, estados.mid, coefs.mid);
    y = aplicarBiquad(y, estados.treble, coefs.treble);
    return y;
}

// 12. Overdrive (soft clipping con tanh)
inline float aplicarOverdrive(float muestra, float dialGain, bool odActivo) {
    if (dialGain < OD_DIAL_MIN) dialGain = OD_DIAL_MIN;
    if (dialGain > OD_DIAL_MAX) dialGain = OD_DIAL_MAX;
    float a = dialGain + (odActivo ? OD_BOOST : 0.0f);
    
    // Tanh solo, sin compensación (el volumen sube con el gain y la salida de tan h ya está acotada entre +-1)
    return std::tanh(a * muestra);
}


// 13. LFO del Chorus con acumulador de fase
inline float lfoChorus(float& faseLFO, float rate, float sampleRate) {
    faseLFO += (2.0f * static_cast<float>(PI) * rate) / sampleRate;
    if (faseLFO >= 2.0f * static_cast<float>(PI)) {
        faseLFO -= 2.0f * static_cast<float>(PI);
    }
    return std::sin(faseLFO);
}

// 14. Leer el buffer circular del Chorus (con interpolación lineal)
inline float leerDelayChorus(const float* buffer, int size, float posicion) {
    int idx0 = static_cast<int>(posicion) % size;
    int idx1 = (idx0 + 1) % size;
    if (idx0 < 0) idx0 += size;
    if (idx1 < 0) idx1 += size;
    float frac = posicion - std::floor(posicion);
    return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
}

// 15. Escribir en el buffer circular del Chorus
inline void guardarBufferChorus(float* buffer, int& writeIndex, float muestra) {
    buffer[writeIndex] = muestra;
    writeIndex = (writeIndex + 1) % CHORUS_BUFFER_SIZE;
}

/// 16. Aplicar el Chorus (Actualizado para usar faseLFO)
inline float aplicarChorus(
    float muestra,
    float* bufferDelay,
    int& writeIndex,
    float& faseLFO, // <-- Ahora es la fase, no el tiempo
    float sampleRate,
    float rate,
    float depth
) {
    guardarBufferChorus(bufferDelay, writeIndex, muestra);
    
    float lfo = lfoChorus(faseLFO, rate, sampleRate);
    float delaySamples = (CHORUS_DELAY_BASE + CHORUS_DELAY_MOD * depth * lfo) * sampleRate;
    
    float readPos = static_cast<float>(writeIndex) - delaySamples;
    while (readPos < 0.0f) readPos += CHORUS_BUFFER_SIZE;
    if (readPos >= CHORUS_BUFFER_SIZE) readPos -= CHORUS_BUFFER_SIZE;
    
    float retrasada = leerDelayChorus(bufferDelay, CHORUS_BUFFER_SIZE, readPos);
    
    return (muestra + retrasada) * 0.5f;
}

// 17. Limitador final (Soft Clip global, sin cambios de volumen entre bloques)
inline float limitarSeñalFinal(float muestra) {
    // Usamos un umbral de 0.9 para dejar un poco de headroom
    constexpr float UMBRAL_LIMITE = 0.9f;
    return std::tanh(muestra / UMBRAL_LIMITE) * UMBRAL_LIMITE;
}

// 18. Convertir a int16_t (con protección defensiva)
inline int16_t convertirPCM(float muestra) {
    // Aunque el limitador ya lo asegura, clampeamos por si acaso
    if (muestra > 1.0f) muestra = 1.0f;
    if (muestra < -1.0f) muestra = -1.0f;
    return static_cast<int16_t>(muestra * MAX_PCM_VALUE);
}

// ============================================================
// CLASE AUDIOPLAYER
// ============================================================

class AudioPlayer {
private:
    HWAVEOUT hWaveOut = nullptr;
    WAVEHDR waveHeader{};
    std::vector<int16_t> bufferPCM;
    std::vector<Nota> notasPista;

    // Estado de los efectos
    EQEstados eqEstados;
    EQCoefs eqCoefs;
    float bufferChorus[CHORUS_BUFFER_SIZE] = {0.0f};
    int writeIndexChorus = 0;
    float tiempoLFO = 0.0f;

    std::atomic<bool> reproduciendo{false};
    std::atomic<bool> pausado{false};
    std::atomic<float> tiempoActual{0.0f};
    std::thread hiloAudio;

    // Variables Atómicas para la UI
    std::atomic<float> paramBass{1.0f};
    std::atomic<float> paramMid{1.0f};
    std::atomic<float> paramTreble{1.0f};
    std::atomic<float> paramGain{0.5f};
    std::atomic<float> paramCRate{1.0f};
    std::atomic<float> paramCDepth{0.5f};
    std::atomic<bool>  odActivo{false};
    std::atomic<int>   paramTipoPastilla{static_cast<int>(TipoPastilla::SINGLE_COIL)};

    void bucleReproduccion();
    void actualizarCoefsEQ();

public:
    AudioPlayer();
    ~AudioPlayer();
    
    bool prepararPista(const std::vector<Nota>& notas, uint32_t sampleRate = 44100);
    void play();
    void stop();
    void pause();
    void resume();

    inline bool estaReproduciendo() const { return reproduciendo.load() && !pausado.load(); }
    inline float obtenerTiempoActual() const { return tiempoActual.load(); }

    inline void setBass(float v) { paramBass.store(v); }
    inline void setMid(float v) { paramMid.store(v); }
    inline void setTreble(float v) { paramTreble.store(v); }
    inline void setGain(float v) { paramGain.store(v); }
    inline void setCRate(float v) { paramCRate.store(v); }
    inline void setCDepth(float v) { paramCDepth.store(v); }
    inline void setODActive(bool activo) { odActivo.store(activo); }
    inline void setTipoPastilla(TipoPastilla tipo) { paramTipoPastilla.store(static_cast<int>(tipo)); }
};

#endif // AUDIO_PLAYER_H