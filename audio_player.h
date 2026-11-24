/*
 * ARCHIVO: audio_player.h / audio_player.cpp
 *
 * CONTEXTO DE LO QUE YA SE TIENE:
 * Se dispone del std::vector<Nota> de la cancion y de los parametros en
 * tiempo real configurados por el usuario (sintetizador, pastilla, EQ,
 * overdrive y chorus).
 *
 * HERRAMIENTAS A USAR:
 * "tipos.h", API multimedia de Windows (<windows.h>, <mmsystem.h> con
 * winmm.lib / waveOut), hilos y atomicos (<thread>, <atomic>), arreglos fijos
 * (<array>) y <cmath> sin asignacion dinamica en el bucle de audio.
 *
 * QUE DEBE HACER ESTE ARCHIVO:
 * Sintetizar el audio muestra por muestra a 44.1 kHz en un hilo separado
 * usando Karplus-Strong o sintesis aditiva (7 armonicos con ADSR), aplicar
 * simulacion de pastillas (Single Coil / Humbucker), filtros Biquad de 3
 * bandas, Overdrive (tanh) y Chorus, y controlar play, pause, stop y seek.
 */




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
#include <memory>

#include "tipos.h"
#include "sintetizador.h"
#include "sintetizador_aditivo.h"
#include "karplus_strong.h"

#pragma comment(lib, "winmm.lib")

// Parametros generales de audio PCM
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr float MAX_PCM_VALUE = 32767.0f;
constexpr double PI = 3.14159265358979323846;

// Rango de ganancia aditiva con tope maximo de 100x
// Gain_dial aporta de 1x a 60x y el boton OD suma +40x fijos
constexpr float GAIN_DIAL_MIN = 1.0f;
constexpr float GAIN_DIAL_MAX = 60.0f;
constexpr float OD_BOOST_FIJO = 40.0f;
constexpr float GAIN_TOTAL_MAX = 100.0f;

// Rango lineal del ecualizador de 3 bandas
constexpr float EQ_GAIN_MIN = 0.1f;
constexpr float EQ_GAIN_MAX = 2.0f;
constexpr float EQ_GAIN_NEUTRO = 1.0f;

// Parametros de Analog Chorus estilo BBD (retardo corto sin timbre ahogado)
constexpr float CHORUS_DELAY_BASE = 0.0055f;
constexpr float CHORUS_DELAY_MOD = 0.0028f;
constexpr int CHORUS_BUFFER_SIZE = 4096;
constexpr float CHORUS_RATE_MIN = 0.15f;
constexpr float CHORUS_RATE_MAX = 4.5f;

// Tamano de bloque de salida y proteccion de picos
constexpr int BLOQUE_MUESTRAS = 4096;
constexpr float FADE_TIME = 0.01f;
constexpr float LIMITADOR_UMBRAL = 0.92f;

// Coeficientes normalizados para filtro Biquad IIR de segundo orden
struct BiquadCoef {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
};

// Registros de retardo por muestra (Direct Form I)
struct BiquadEstado {
    float x1 = 0.0f, x2 = 0.0f;
    float y1 = 0.0f, y2 = 0.0f;
};

// Banco de coeficientes para ecualizador de 3 bandas
struct EQCoefs {
    BiquadCoef bass;
    BiquadCoef mid;
    BiquadCoef treble;
};

// Estados internos del ecualizador de 3 bandas
struct EQEstados {
    BiquadEstado bass;
    BiquadEstado mid;
    BiquadEstado treble;
};

//====================
//y de nuevo, con los "===".
//SIMULACIÓN DE SONIDO REALISTA (KARPLUS STRONG)

// Banco de 5 filtros Biquad en cascada que modelan un gabinete 4x12 Celestion V30
struct CabSimCoefs {
    BiquadCoef hpSub;         // Corte subsonico en 75 Hz para limpiar graves sueltos
    BiquadCoef resCaja;       // Resonancia de caja cerrada alrededor de 98 Hz
    BiquadCoef scoopMedios;   // Atenuacion de medios-graves encajonados en 360 Hz
    BiquadCoef presenciaCono; // Pico caracteristico de presencia del cono en 2400 Hz
    BiquadCoef rolloffCono;   // Caida natural del altavoz por encima de 4200 Hz
};

// Registros de estado para las 5 etapas del CabSim compartido
struct CabSimEstados {
    BiquadEstado hpSub;
    BiquadEstado resCaja;
    BiquadEstado scoopMedios;
    BiquadEstado presenciaCono;
    BiquadEstado rolloffCono;
};

// Estado interno de la etapa de Overdrive/Distortion dinamica
struct OverdriveEstado {
    float hpPrevIn = 0.0f;
    float hpPrevOut = 0.0f;
    float envolvente = 0.0f;
    float dcBlockIn = 0.0f;
    float dcBlockOut = 0.0f;
};

//======================
// Convierte muestra flotante [-1, 1] a entero PCM de 16 bits con saturacion segura
inline int16_t convertirPCM(float muestra) {
    muestra = std::clamp(muestra, -1.0f, 1.0f);
    return static_cast<int16_t>(muestra * MAX_PCM_VALUE);
}

// Evalua una muestra a traves de un filtro Biquad y actualiza su historial
inline float aplicarBiquad(float x, BiquadEstado& s, const BiquadCoef& c) {
    const float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
    s.x2 = s.x1;
    s.x1 = x;
    s.y2 = s.y1;
    s.y1 = y;
    return y;
}

// Fase 8/9: Etapa de Overdrive con ganancia aditiva (1x..100x) y bias dinamico
inline float aplicarOverdrive(float muestra, float gainDial, bool odBoost, OverdriveEstado& st) {
    const float driveTotal = std::clamp(
        std::max(1.0f, gainDial) + (odBoost ? OD_BOOST_FIJO : 0.0f),
        1.0f,
        GAIN_TOTAL_MAX
    );

    // Si no hay ganancia aplicada, mantener el historial sincronizado en bypass
    if (driveTotal <= 1.05f) {
        st.hpPrevIn = muestra;
        st.hpPrevOut = muestra;
        st.envolvente *= 0.99f;
        st.dcBlockIn = muestra;
        st.dcBlockOut = muestra;
        return muestra;
    }

    // 1. Pre-filtro pasa-altos (~115 Hz) para evitar saturacion fofa en cuerdas graves
    const float hp = 0.983f * (st.hpPrevOut + muestra - st.hpPrevIn);
    st.hpPrevIn = muestra;
    st.hpPrevOut = hp;

    // 2. Detector de envolvente rapido para desplazar el punto de polarizacion (tube sag)
    const float absIn = std::abs(hp);
    const float coefEnv = (absIn > st.envolvente) ? 0.04f : 0.0008f;
    st.envolvente += coefEnv * (absIn - st.envolvente);

    // 3. Primera etapa: saturacion asimetrica suave con bias dinamico (armonicos pares)
    const float biasDinamico = 0.05f + 0.14f * std::clamp(st.envolvente * 2.5f, 0.0f, 1.0f);
    const float pre1 = hp * (driveTotal * 0.34f) + biasDinamico;
    const float et1 = std::tanh(pre1) - std::tanh(biasDinamico);

    // 4. Segunda etapa: recorte mas firme tipo diodos de silicio con compensacion de nivel
    const float impulsoEt2 = 1.35f + (driveTotal / GAIN_TOTAL_MAX) * 2.4f;
    const float pre2 = et1 * impulsoEt2;
    const float clipped = std::clamp(std::tanh(pre2 * 1.25f), -0.78f, 0.72f);

    // 5. Bloqueador DC para centrar la onda asimetrica antes del EQ y el CabSim
    const float dcOut = 0.995f * (st.dcBlockOut + clipped - st.dcBlockIn);
    st.dcBlockIn = clipped;
    st.dcBlockOut = dcOut;

    return dcOut * 0.92f;
}

// Ecualizador parametrico de 3 bandas (Bass, Mid, Treble)
inline float aplicarEQ(float muestra, EQEstados& estados, const EQCoefs& coefs) {
    float y = aplicarBiquad(muestra, estados.bass, coefs.bass);
    y = aplicarBiquad(y, estados.mid, coefs.mid);
    y = aplicarBiquad(y, estados.treble, coefs.treble);
    return y;
}

// Fase 6/7: CabSim compartido de gabinete 4x12 aplicado a toda la cadena
inline float aplicarCabSim(float muestra, CabSimEstados& st, const CabSimCoefs& c) {
    float y = aplicarBiquad(muestra, st.hpSub, c.hpSub);
    y = aplicarBiquad(y, st.resCaja, c.resCaja);
    y = aplicarBiquad(y, st.scoopMedios, c.scoopMedios);
    y = aplicarBiquad(y, st.presenciaCono, c.presenciaCono);
    y = aplicarBiquad(y, st.rolloffCono, c.rolloffCono);
    return y;
}

// Oscilador de baja frecuencia senoidal para modulacion del chorus
inline float lfoChorus(float& faseLFO, float rate, float sampleRate) {
    faseLFO += (2.0f * static_cast<float>(PI) * rate) / sampleRate;
    if (faseLFO >= 2.0f * static_cast<float>(PI)) {
        faseLFO -= 2.0f * static_cast<float>(PI);
    }
    return std::sin(faseLFO);
}

// Lectura con interpolacion lineal en el buffer circular del chorus
inline float leerDelayChorus(const float* buffer, int size, float posicion) {
    while (posicion < 0.0f) posicion += static_cast<float>(size);
    while (posicion >= static_cast<float>(size)) posicion -= static_cast<float>(size);

    const int idx0 = static_cast<int>(posicion);
    const int idx1 = (idx0 + 1) % size;
    const float frac = posicion - static_cast<float>(idx0);
    return buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;
}

// Inserta una nueva muestra en el buffer circular del chorus
inline void guardarBufferChorus(float* buffer, int& writeIndex, float muestra) {
    buffer[writeIndex] = muestra;
    writeIndex = (writeIndex + 1) % CHORUS_BUFFER_SIZE;
}

// Analog Chorus estilo BBD con mezcla 65% seca y 35% humeda filtrada
inline float aplicarAnalogChorus(
    float muestra,
    float* bufferDelay,
    int& writeIndex,
    float& faseLFO,
    float& filtroBBD,
    float sampleRate,
    float rate,
    float depth
) {
    filtroBBD += 0.32f * (muestra - filtroBBD);
    guardarBufferChorus(bufferDelay, writeIndex, filtroBBD);

    const float lfo = lfoChorus(faseLFO, rate, sampleRate);
    depth = std::clamp(depth, 0.0f, 1.0f);
    const float delaySamples = (CHORUS_DELAY_BASE + CHORUS_DELAY_MOD * depth * lfo) * sampleRate;

    const float readPos = static_cast<float>(writeIndex) - delaySamples;
    const float retrasada = leerDelayChorus(bufferDelay, CHORUS_BUFFER_SIZE, readPos);

    return muestra * 0.65f + retrasada * 0.35f;
}

// Limitador suave final para evitar clipping  duro en la tarjeta de sonido
inline float limitarSeñalFinal(float muestra) {
    return std::tanh(muestra / LIMITADOR_UMBRAL) * LIMITADOR_UMBRAL;
}


//TODOS esos meses aprendiendo de audio por fin sirven pa algo
class AudioPlayer {
private:
    HWAVEOUT hWaveOut = nullptr;
    std::vector<Nota> notasPista;

    // Estados DSP persistentes entre bloques de reproduccion
    OverdriveEstado overdriveEstado{};
    EQEstados eqEstados{};
    EQCoefs eqCoefs{};
    CabSimEstados cabEstados{};
    CabSimCoefs cabCoefs{};

    float bufferChorus[CHORUS_BUFFER_SIZE]{};
    int writeIndexChorus = 0;
    float faseLFO = 0.0f;
    float filtroBBDChorus = 0.0f;

    // Sincronizacion del hilo de audio con la interfaz Qt
    std::atomic<bool> reproduciendo{false};
    std::atomic<bool> pausado{false};
    std::atomic<float> tiempoActual{0.0f};
    std::atomic<float> paramSeek{-1.0f};
    std::thread hiloAudio;

    // Parametros atomicos modificables en tiempo real desde la UI
    std::atomic<int> paramSintetizador{static_cast<int>(TipoSintetizador::KARPLUS_STRONG)};
    std::atomic<int> paramTipoPastilla{static_cast<int>(TipoPastilla::SINGLE_COIL)};
    std::atomic<float> paramBass{EQ_GAIN_NEUTRO};
    std::atomic<float> paramMid{EQ_GAIN_NEUTRO};
    std::atomic<float> paramTreble{EQ_GAIN_NEUTRO};
    std::atomic<float> paramOutputLevel{0.90f};
    std::atomic<float> paramGainDial{1.0f};
    std::atomic<float> paramCRate{0.8f};
    std::atomic<float> paramCDepth{0.45f};
    std::atomic<bool> odActivo{false};
    std::atomic<bool> chorusActivo{true};

    void bucleReproduccion();
    void actualizarCoefsEQ();
    void inicializarCoefsCabSim();
    void resetearEstadosDSP();

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

    inline void setSintetizador(TipoSintetizador tipo) { paramSintetizador.store(static_cast<int>(tipo)); }
    inline void setTipoPastilla(TipoPastilla tipo) { paramTipoPastilla.store(static_cast<int>(tipo)); }
    inline void setBass(float v) { paramBass.store(std::clamp(v, EQ_GAIN_MIN, EQ_GAIN_MAX)); }
    inline void setMid(float v) { paramMid.store(std::clamp(v, EQ_GAIN_MIN, EQ_GAIN_MAX)); }
    inline void setTreble(float v) { paramTreble.store(std::clamp(v, EQ_GAIN_MIN, EQ_GAIN_MAX)); }
    inline void setGain(float v) { paramOutputLevel.store(std::clamp(v, 0.0f, 2.0f)); }
    inline void setODDrive(float v) { paramGainDial.store(std::clamp(v, GAIN_DIAL_MIN, GAIN_DIAL_MAX)); }
    inline void setCRate(float v) { paramCRate.store(std::clamp(v, CHORUS_RATE_MIN, CHORUS_RATE_MAX)); }
    inline void setCDepth(float v) { paramCDepth.store(std::clamp(v, 0.0f, 1.0f)); }
    inline void setODActive(bool activo) { odActivo.store(activo); }
    inline void setChorusActive(bool activo) { chorusActivo.store(activo); }
};

#endif // AUDIO_PLAYER_H