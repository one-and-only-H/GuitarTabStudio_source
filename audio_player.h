#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>
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

// EQ: ganancia lineal de cada banda
constexpr float EQ_GAIN_MIN = 0.1f;
constexpr float EQ_GAIN_MAX = 2.0f;
constexpr float EQ_GAIN_NEUTRO = 1.0f;

// Chorus
constexpr float CHORUS_DELAY_BASE = 0.015f;
constexpr float CHORUS_DELAY_MOD = 0.005f;
constexpr int CHORUS_BUFFER_SIZE = 4096;
constexpr float CHORUS_RATE_MIN = 0.1f;
constexpr float CHORUS_RATE_MAX = 5.0f;

// Audio
constexpr int BLOQUE_MUESTRAS = 2048;
constexpr float FADE_TIME = 0.01f; // 10 ms para el micro-fade del seek
constexpr float LIMITADOR_UMBRAL = 0.9f;

// ============================================================
// STRUCTS
// ============================================================

enum class TipoPastilla { SINGLE_COIL, HUMBUCKER };
enum class TipoAtaque { LET_RING, MUTED };
enum class EstadoADSR { ATTACK, DECAY, SUSTAIN, RELEASE, OFF };

struct PerfilArmonico {
    float pesos[NUM_HARMONICS]{};
    float tasasDecaimiento[NUM_HARMONICS]{};
    float ataques[NUM_HARMONICS]{};
    float sustentos[NUM_HARMONICS]{};
    float releases[NUM_HARMONICS]{};
};

struct EnvolventeADSR {
    EstadoADSR estado = EstadoADSR::OFF;
    float valorActual = 0.0f;
    float valorInicioRelease = 0.0f;
    float tiempoEnEstado = 0.0f;
};

struct NotaSonando {
    int pitchOriginal = -1;
    float duracionTotal = 0.0f;
    float tiempoTranscurrido = 0.0f;
    float timestampInicio = 0.0f;
    float volumenVelocity = 0.0f;

    float frecuencias[NUM_HARMONICS]{};
    float fases[NUM_HARMONICS]{};
    EnvolventeADSR envolventes[NUM_HARMONICS]{};
    PerfilArmonico perfil{};
};

// --- EQ (secciones biquad) ---
struct BiquadCoef {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
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

inline int16_t convertirPCM(float muestra) {
    muestra = std::clamp(muestra, -1.0f, 1.0f);
    return static_cast<int16_t>(muestra * MAX_PCM_VALUE);
}

inline float obtenerFrecuencia(int pitch) {
    return 440.0f * std::pow(2.0f, (pitch - 69) / 12.0f);
}

inline std::array<float, NUM_HARMONICS> dividirEnArmonicos(float frecuenciaFundamental) {
    std::array<float, NUM_HARMONICS> armonicos{};
    for (int i = 0; i < NUM_HARMONICS; i++) {
        armonicos[i] = frecuenciaFundamental * static_cast<float>(i + 1);
    }
    return armonicos;
}

inline float generarOndaArmonico(float frecuencia, float& fase, float sampleRate) {
    const float valor = std::sin(fase);
    const float incrementoFase = (2.0f * static_cast<float>(PI) * frecuencia) / sampleRate;
    fase = std::fmod(fase + incrementoFase, 2.0f * static_cast<float>(PI));
    return valor;
}

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

inline PerfilArmonico obtenerPerfil(TipoPastilla pastilla, TipoAtaque ataque) {
    PerfilArmonico perfil{};

    if (pastilla == TipoPastilla::SINGLE_COIL) {
        const float p[] = {1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
        const float d[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        std::copy(p, p + NUM_HARMONICS, perfil.pesos);
        std::copy(d, d + NUM_HARMONICS, perfil.tasasDecaimiento);
    } else {
        const float p[] = {1.0f, 0.9f, 0.6f, 0.3f, 0.2f, 0.1f, 0.05f};
        const float d[] = {1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};
        std::copy(p, p + NUM_HARMONICS, perfil.pesos);
        std::copy(d, d + NUM_HARMONICS, perfil.tasasDecaimiento);
    }

    if (ataque == TipoAtaque::LET_RING) {
        const float a[] = {0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f};
        const float s[] = {0.80f, 0.70f, 0.60f, 0.50f, 0.40f, 0.30f, 0.20f};
        const float r[] = {0.50f, 0.40f, 0.30f, 0.25f, 0.20f, 0.15f, 0.10f};
        std::copy(a, a + NUM_HARMONICS, perfil.ataques);
        std::copy(s, s + NUM_HARMONICS, perfil.sustentos);
        std::copy(r, r + NUM_HARMONICS, perfil.releases);
    } else {
        const float a[] = {0.003f, 0.003f, 0.003f, 0.003f, 0.003f, 0.003f, 0.003f};
        const float s[] = {0.10f, 0.08f, 0.05f, 0.03f, 0.02f, 0.01f, 0.00f};
        const float r[] = {0.05f, 0.04f, 0.03f, 0.02f, 0.01f, 0.01f, 0.01f};
        std::copy(a, a + NUM_HARMONICS, perfil.ataques);
        std::copy(s, s + NUM_HARMONICS, perfil.sustentos);
        std::copy(r, r + NUM_HARMONICS, perfil.releases);
    }

    return perfil;
}

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
    const float dt = 1.0f / sampleRate;

    // La nota puede terminar durante ATTACK o DECAY, no solo en SUSTAIN.
    if (tiempoTranscurrido >= duracionTotal &&
        env.estado != EstadoADSR::RELEASE && env.estado != EstadoADSR::OFF) {
        env.valorInicioRelease = env.valorActual;
        env.estado = EstadoADSR::RELEASE;
        env.tiempoEnEstado = 0.0f;
    }

    env.tiempoEnEstado += dt;

    switch (env.estado) {
        case EstadoADSR::ATTACK: {
            const float ataque = std::max(0.001f, tiempoAtaque);
            env.valorActual = std::min(1.0f, env.tiempoEnEstado / ataque);
            if (env.valorActual >= 1.0f) {
                env.valorActual = 1.0f;
                env.estado = EstadoADSR::DECAY;
                env.tiempoEnEstado = 0.0f;
            }
            break;
        }
        case EstadoADSR::DECAY: {
            const float decay = std::max(0.001f, 0.5f * tasaDecaimiento);
            const float progreso = std::min(1.0f, env.tiempoEnEstado / decay);
            env.valorActual = 1.0f - ((1.0f - nivelSustain) * progreso);
            if (progreso >= 1.0f) {
                env.valorActual = nivelSustain;
                env.estado = EstadoADSR::SUSTAIN;
                env.tiempoEnEstado = 0.0f;
            }
            break;
        }
        case EstadoADSR::SUSTAIN:
            env.valorActual = nivelSustain;
            break;
        case EstadoADSR::RELEASE: {
            const float release = std::max(0.001f, tiempoRelease);
            const float progreso = std::min(1.0f, env.tiempoEnEstado / release);
            env.valorActual = env.valorInicioRelease * (1.0f - progreso);
            if (progreso >= 1.0f) {
                env.valorActual = 0.0f;
                env.estado = EstadoADSR::OFF;
            }
            break;
        }
        case EstadoADSR::OFF:
            env.valorActual = 0.0f;
            break;
    }

    return env.valorActual;
}

inline void aplicarEnvolventes(
    float presamples[NUM_HARMONICS],
    EnvolventeADSR envolventes[NUM_HARMONICS],
    const PerfilArmonico& perfil,
    float duracionTotal,
    float tiempoTranscurrido,
    float sampleRate
) {
    for (int i = 0; i < NUM_HARMONICS; i++) {
        const float valorEnv = avanzarADSR(
            envolventes[i], perfil.tasasDecaimiento[i], perfil.ataques[i],
            perfil.sustentos[i], perfil.releases[i], duracionTotal,
            tiempoTranscurrido, sampleRate
        );
        presamples[i] *= valorEnv;
    }
}

inline float mezclarArmonicos(const float presamples[NUM_HARMONICS], const float pesos[NUM_HARMONICS]) {
    float suma = 0.0f;
    for (int i = 0; i < NUM_HARMONICS; i++) {
        suma += presamples[i] * pesos[i];
    }
    return suma / static_cast<float>(NUM_HARMONICS);
}

inline float normalizar(float suma, int N) {
    if (N < 1) N = 1;
    return suma / std::sqrt(static_cast<float>(N));
}

inline float aplicarBiquad(float x, BiquadEstado& s, const BiquadCoef& c) {
    const float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
    s.x2 = s.x1;
    s.x1 = x;
    s.y2 = s.y1;
    s.y1 = y;
    return y;
}

inline float aplicarEQ(float muestra, EQEstados& estados, const EQCoefs& coefs) {
    float y = aplicarBiquad(muestra, estados.bass, coefs.bass);
    y = aplicarBiquad(y, estados.mid, coefs.mid);
    y = aplicarBiquad(y, estados.treble, coefs.treble);
    return y;
}

inline float aplicarOverdrive(float muestra, float drive, bool odActivo) {
    if (!odActivo) return muestra;
    drive = std::clamp(drive, OD_DIAL_MIN, OD_DIAL_MAX);
    return std::tanh(drive * muestra);
}

inline float lfoChorus(float& faseLFO, float rate, float sampleRate) {
    faseLFO += (2.0f * static_cast<float>(PI) * rate) / sampleRate;
    faseLFO = std::fmod(faseLFO, 2.0f * static_cast<float>(PI));
    return std::sin(faseLFO);
}

inline float leerDelayChorus(const float* buffer, int size, float posicion) {
    const float posicionNormalizada = std::fmod(std::max(0.0f, posicion), static_cast<float>(size));
    const int idx0 = static_cast<int>(posicionNormalizada);
    const int idx1 = (idx0 + 1) % size;
    const float frac = posicionNormalizada - static_cast<float>(idx0);
    return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
}

inline void guardarBufferChorus(float* buffer, int& writeIndex, float muestra) {
    buffer[writeIndex] = muestra;
    writeIndex = (writeIndex + 1) % CHORUS_BUFFER_SIZE;
}

inline float aplicarChorus(
    float muestra,
    float* bufferDelay,
    int& writeIndex,
    float& faseLFO,
    float sampleRate,
    float rate,
    float depth
) {
    guardarBufferChorus(bufferDelay, writeIndex, muestra);
    const float lfo = lfoChorus(faseLFO, rate, sampleRate);
    depth = std::clamp(depth, 0.0f, 1.0f);
    const float delaySamples = (CHORUS_DELAY_BASE + CHORUS_DELAY_MOD * depth * lfo) * sampleRate;

    float readPos = static_cast<float>(writeIndex) - delaySamples;
    while (readPos < 0.0f) readPos += CHORUS_BUFFER_SIZE;
    while (readPos >= CHORUS_BUFFER_SIZE) readPos -= CHORUS_BUFFER_SIZE;

    const float retrasada = leerDelayChorus(bufferDelay, CHORUS_BUFFER_SIZE, readPos);
    return (muestra + retrasada) * 0.5f;
}

inline float limitarSeñalFinal(float muestra) {
    return std::tanh(muestra / LIMITADOR_UMBRAL) * LIMITADOR_UMBRAL;
}

// ============================================================
// CLASE AUDIOPLAYER
// ============================================================

class AudioPlayer {
private:
    HWAVEOUT hWaveOut = nullptr;
    std::vector<int16_t> bufferPCM;
    std::vector<Nota> notasPista;

    EQEstados eqEstados{};
    EQCoefs eqCoefs{};
    float bufferChorus[CHORUS_BUFFER_SIZE]{};
    int writeIndexChorus = 0;
    float faseLFO = 0.0f;

    std::atomic<bool> reproduciendo{false};
    std::atomic<bool> pausado{false};
    std::atomic<float> tiempoActual{0.0f};
    std::atomic<float> paramSeek{-1.0f};
    std::thread hiloAudio;

    // Controles de la interfaz
    std::atomic<float> paramBass{EQ_GAIN_NEUTRO};
    std::atomic<float> paramMid{EQ_GAIN_NEUTRO};
    std::atomic<float> paramTreble{EQ_GAIN_NEUTRO};
    std::atomic<float> paramGain{0.5f};       // Volumen de salida, independiente del drive
    std::atomic<float> paramODDrive{1.0f};    // Intensidad de saturación
    std::atomic<float> paramCRate{1.0f};
    std::atomic<float> paramCDepth{0.5f};
    std::atomic<bool> odActivo{false};
    std::atomic<bool> chorusActivo{true};
    std::atomic<int> paramTipoPastilla{static_cast<int>(TipoPastilla::SINGLE_COIL)};

    void bucleReproduccion();
    void actualizarCoefsEQ();

public:
    AudioPlayer();
    ~AudioPlayer();

    bool prepararPista(const std::vector<Nota>& notas, uint32_t sampleRate = SAMPLE_RATE);
    void play();
    void stop();
    void pause();
    void resume();
    void seek(float segundo);

    inline bool estaReproduciendo() const { return reproduciendo.load() && !pausado.load(); }
    inline float obtenerTiempoActual() const { return tiempoActual.load(); }

    inline void setBass(float v) { paramBass.store(std::clamp(v, EQ_GAIN_MIN, EQ_GAIN_MAX)); }
    inline void setMid(float v) { paramMid.store(std::clamp(v, EQ_GAIN_MIN, EQ_GAIN_MAX)); }
    inline void setTreble(float v) { paramTreble.store(std::clamp(v, EQ_GAIN_MIN, EQ_GAIN_MAX)); }
    inline void setGain(float v) { paramGain.store(std::clamp(v, 0.0f, 2.0f)); }
    inline void setODDrive(float v) { paramODDrive.store(std::clamp(v, OD_DIAL_MIN, OD_DIAL_MAX)); }
    inline void setCRate(float v) { paramCRate.store(std::clamp(v, CHORUS_RATE_MIN, CHORUS_RATE_MAX)); }
    inline void setCDepth(float v) { paramCDepth.store(std::clamp(v, 0.0f, 1.0f)); }
    inline void setODActive(bool activo) { odActivo.store(activo); }
    inline void setChorusActive(bool activo) { chorusActivo.store(activo); }
    inline void setTipoPastilla(TipoPastilla tipo) { paramTipoPastilla.store(static_cast<int>(tipo)); }
};

#endif // AUDIO_PLAYER_H
