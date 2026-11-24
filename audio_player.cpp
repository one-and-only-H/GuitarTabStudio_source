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



#include "audio_player.h"

namespace {

// Normaliza coeficientes Biquad dividiendo por a0 para estabilidad numerica
BiquadCoef normalizarCoeficientes(float b0, float b1, float b2,
                                  float a0, float a1, float a2) {
    if (std::abs(a0) < 1.0e-12f) return BiquadCoef{};
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

// Filtro pasa-bajos de 2 polos (de Audio EQ Cookbook)
BiquadCoef lowPass(float frecuencia, float Q, float sampleRate) {
    const float w0 = 2.0f * static_cast<float>(PI) * frecuencia / sampleRate;
    const float c = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * Q);

    const float b0 = (1.0f - c) * 0.5f;
    const float b1 = 1.0f - c;
    const float b2 = (1.0f - c) * 0.5f;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * c;
    const float a2 = 1.0f - alpha;
    return normalizarCoeficientes(b0, b1, b2, a0, a1, a2);
}

// Filtro pasa-altos de 2 polos (Audio EQ Cookbook)
BiquadCoef highPass(float frecuencia, float Q, float sampleRate) {
    const float w0 = 2.0f * static_cast<float>(PI) * frecuencia / sampleRate;
    const float c = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * Q);

    const float b0 = (1.0f + c) * 0.5f;
    const float b1 = -(1.0f + c);
    const float b2 = (1.0f + c) * 0.5f;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * c;
    const float a2 = 1.0f - alpha;
    return normalizarCoeficientes(b0, b1, b2, a0, a1, a2);
}

// Filtro Low-Shelf para control de graves del ecualizador
BiquadCoef lowShelf(float frecuencia, float ganancia, float sampleRate) {
    const float A = std::sqrt(std::max(0.001f, ganancia));
    const float w0 = 2.0f * static_cast<float>(PI) * frecuencia / sampleRate;
    const float c = std::cos(w0);
    const float s = std::sin(w0);
    const float alpha = (s * 0.5f) * std::sqrt(2.0f);
    const float beta = 2.0f * std::sqrt(A) * alpha;

    const float b0 = A * ((A + 1.0f) - (A - 1.0f) * c + beta);
    const float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * c);
    const float b2 = A * ((A + 1.0f) - (A - 1.0f) * c - beta);
    const float a0 = (A + 1.0f) + (A - 1.0f) * c + beta;
    const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * c);
    const float a2 = (A + 1.0f) + (A - 1.0f) * c - beta;
    return normalizarCoeficientes(b0, b1, b2, a0, a1, a2);
}

// Filtro High-Shelf para control de agudos del ecualizador
BiquadCoef highShelf(float frecuencia, float ganancia, float sampleRate) {
    const float A = std::sqrt(std::max(0.001f, ganancia));
    const float w0 = 2.0f * static_cast<float>(PI) * frecuencia / sampleRate;
    const float c = std::cos(w0);
    const float s = std::sin(w0);
    const float alpha = (s * 0.5f) * std::sqrt(2.0f);
    const float beta = 2.0f * std::sqrt(A) * alpha;

    const float b0 = A * ((A + 1.0f) + (A - 1.0f) * c + beta);
    const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * c);
    const float b2 = A * ((A + 1.0f) + (A - 1.0f) * c - beta);
    const float a0 = (A + 1.0f) - (A - 1.0f) * c + beta;
    const float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * c);
    const float a2 = (A + 1.0f) - (A - 1.0f) * c - beta;
    return normalizarCoeficientes(b0, b1, b2, a0, a1, a2);
}

// Filtro Peaking EQ para realce o corte en frecuencias especificas
BiquadCoef peakingEQ(float frecuencia, float ganancia, float Q, float sampleRate) {
    const float A = std::sqrt(std::max(0.001f, ganancia));
    const float w0 = 2.0f * static_cast<float>(PI) * frecuencia / sampleRate;
    const float c = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * Q);

    const float b0 = 1.0f + alpha * A;
    const float b1 = -2.0f * c;
    const float b2 = 1.0f - alpha * A;
    const float a0 = 1.0f + alpha / A;
    const float a1 = -2.0f * c;
    const float a2 = 1.0f - alpha / A;
    return normalizarCoeficientes(b0, b1, b2, a0, a1, a2);
}

// Fabrica polimorfica para instanciar el motor de sintesis activo
//viste que con palabras bien rebuscadas suena más dificil?
//ajá pero vas a entregar con eso así y el poco de palabras esas?
//si



//y esto de donde?
//ahi si que la IA ayude, ya todo el mes tirando if else while std:: 
//no me voy a pelear con la api de windows, tu? 
//bueno dale pues
//igual hice bastante de las funciones inline
//y pq seguimos hablando aqui y no wsp?
std::unique_ptr<ISintetizador> crearSintetizador(TipoSintetizador tipo, uint32_t sampleRate) {
    if (tipo == TipoSintetizador::ADITIVO) {
        return std::make_unique<SintetizadorAditivo>(sampleRate);
    }
    return std::make_unique<SintetizadorKarplus>(sampleRate);
}

} // namespace

AudioPlayer::AudioPlayer() {
    actualizarCoefsEQ();
    inicializarCoefsCabSim();
}

AudioPlayer::~AudioPlayer() {
    stop();
}

// Configura la curva de respuesta en frecuencia de un gabinete 4x12 Celestion V30
void AudioPlayer::inicializarCoefsCabSim() {
    const float sr = static_cast<float>(SAMPLE_RATE);
    cabCoefs.hpSub         = highPass(75.0f, 0.707f, sr);
    cabCoefs.resCaja       = peakingEQ(98.0f, 1.48f, 1.80f, sr);   // +3.4 dB cuerpo de caja
    cabCoefs.scoopMedios   = peakingEQ(360.0f, 0.60f, 1.25f, sr);  // -4.4 dB limpieza de medios
    cabCoefs.presenciaCono = peakingEQ(2400.0f, 1.55f, 1.65f, sr); // +3.8 dB mordida de guitarra
    cabCoefs.rolloffCono   = lowPass(4200.0f, 0.90f, sr);          // Caida natural del cono
}

// Limpia los historiales de filtros y lineas de retardo para evitar clics o residuos
void AudioPlayer::resetearEstadosDSP() {
    overdriveEstado = OverdriveEstado{};
    eqEstados = EQEstados{};
    cabEstados = CabSimEstados{};
    writeIndexChorus = 0;
    faseLFO = 0.0f;
    filtroBBDChorus = 0.0f;
    std::fill(std::begin(bufferChorus), std::end(bufferChorus), 0.0f);
}

bool AudioPlayer::prepararPista(const std::vector<Nota>& notas, uint32_t sampleRate) {
    if (reproduciendo.load()) return false;
    if (sampleRate != SAMPLE_RATE) return false;

    if (hiloAudio.joinable()) hiloAudio.join();

    notasPista = notas;
    std::sort(notasPista.begin(), notasPista.end(),
              [](const Nota& a, const Nota& b) { return a.timestamp < b.timestamp; });

    resetearEstadosDSP();
    tiempoActual.store(0.0f);
    paramSeek.store(-1.0f);
    return true;
}

void AudioPlayer::play() {
    if (reproduciendo.load()) return;
    if (hiloAudio.joinable()) hiloAudio.join();

    paramSeek.store(-1.0f);
    reproduciendo.store(true);
    pausado.store(false);
    tiempoActual.store(0.0f);
    hiloAudio = std::thread(&AudioPlayer::bucleReproduccion, this);
}

void AudioPlayer::pause() {
    pausado.store(true);
}

void AudioPlayer::resume() {
    pausado.store(false);
}

void AudioPlayer::stop() {
    reproduciendo.store(false);
    pausado.store(false);
    if (hiloAudio.joinable()) hiloAudio.join();
}

void AudioPlayer::seek(float segundo) {
    if (!reproduciendo.load() || !std::isfinite(segundo)) return;
    paramSeek.store(std::max(0.0f, segundo));
}

void AudioPlayer::actualizarCoefsEQ() {
    const float bass = std::clamp(paramBass.load(), EQ_GAIN_MIN, EQ_GAIN_MAX);
    const float mid = std::clamp(paramMid.load(), EQ_GAIN_MIN, EQ_GAIN_MAX);
    const float treble = std::clamp(paramTreble.load(), EQ_GAIN_MIN, EQ_GAIN_MAX);

    eqCoefs.bass = lowShelf(250.0f, bass, static_cast<float>(SAMPLE_RATE));
    eqCoefs.mid = peakingEQ(1000.0f, mid, 0.707f, static_cast<float>(SAMPLE_RATE));
    eqCoefs.treble = highShelf(4000.0f, treble, static_cast<float>(SAMPLE_RATE));
}

void AudioPlayer::bucleReproduccion() {
    // 1. Apertura del dispositivo de salida de audio Windows WaveOut
    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        hWaveOut = nullptr;
        reproduciendo.store(false);
        return;
    }

    // 2. Preparacion de triple buffer circular para reproduccion continua sin cortes
    constexpr int NUM_BUFFERS = 3;
    std::vector<int16_t> buffersPCM[NUM_BUFFERS];
    WAVEHDR headers[NUM_BUFFERS]{};

    for (int b = 0; b < NUM_BUFFERS; ++b) {
        buffersPCM[b].resize(BLOQUE_MUESTRAS, 0);
        headers[b].lpData = reinterpret_cast<LPSTR>(buffersPCM[b].data());
        headers[b].dwBufferLength = static_cast<DWORD>(BLOQUE_MUESTRAS * sizeof(int16_t));

        if (waveOutPrepareHeader(hWaveOut, &headers[b], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            for (int j = 0; j < b; ++j) {
                waveOutUnprepareHeader(hWaveOut, &headers[j], sizeof(WAVEHDR));
            }
            waveOutClose(hWaveOut);
            hWaveOut = nullptr;
            reproduciendo.store(false);
            return;
        }
        headers[b].dwFlags |= WHDR_DONE;
    }

    // 3. Instanciacion del motor de sintesis inicial (Karplus-Strong o Aditivo)
    TipoSintetizador tipoSynthActual = static_cast<TipoSintetizador>(paramSintetizador.load());
    std::unique_ptr<ISintetizador> synth = crearSintetizador(tipoSynthActual, SAMPLE_RATE);

    int bufIdx = 0;
    double tiempoCursor = 0.0;
    size_t indiceNotaActual = 0;
    float bufferFloat[BLOQUE_MUESTRAS]{};

    // Variables de control de crossfade suave durante operaciones de seek
    int estadoFade = 0;
    float multiplicadorFade = 1.0f;
    const float pasoFade = 1.0f / (SAMPLE_RATE * FADE_TIME);
    bool dispositivoPausado = false;

    auto sincronizarPausa = [&]() {
        if (pausado.load() && !dispositivoPausado) {
            if (waveOutPause(hWaveOut) == MMSYSERR_NOERROR) dispositivoPausado = true;
        } else if (!pausado.load() && dispositivoPausado) {
            if (waveOutRestart(hWaveOut) == MMSYSERR_NOERROR) dispositivoPausado = false;
        }
    };

    while (reproduciendo.load()) {
        while (reproduciendo.load() && pausado.load()) {
            sincronizarPausa();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        sincronizarPausa();
        if (!reproduciendo.load()) break;

        // 4. Conmutacion en caliente de sintetizador reconstruyendo las notas activas
        const TipoSintetizador tipoSolicitado = static_cast<TipoSintetizador>(paramSintetizador.load());
        if (tipoSolicitado != tipoSynthActual) {
            tipoSynthActual = tipoSolicitado;
            synth = crearSintetizador(tipoSynthActual, SAMPLE_RATE);
            synth->configurarPastilla(static_cast<TipoPastilla>(paramTipoPastilla.load()));

            for (size_t i = 0; i < indiceNotaActual && i < notasPista.size(); ++i) {
                const Nota& nota = notasPista[i];
                const float elapsed = static_cast<float>(tiempoCursor - nota.timestamp);
                if (elapsed >= 0.0f && elapsed < nota.duracion + 0.4f) {
                    const float vel = std::clamp(static_cast<float>(nota.velocidad) / 127.0f, 0.0f, 1.0f);
                    synth->noteOn(nota.pitch, vel, static_cast<float>(nota.duracion), elapsed);
                }
            }
        }

        // Lectura atomica de los controles de la interfaz para este bloque
        const TipoPastilla pastillaActual = static_cast<TipoPastilla>(paramTipoPastilla.load());
        synth->configurarPastilla(pastillaActual);

        const float outputLevel = paramOutputLevel.load();
        const float gainDialActual = paramGainDial.load();
        const bool odBoostActual = odActivo.load();
        const bool chorusActivoActual = chorusActivo.load();
        const float rateActual = std::clamp(paramCRate.load(), CHORUS_RATE_MIN, CHORUS_RATE_MAX);
        const float depthActual = std::clamp(paramCDepth.load(), 0.0f, 1.0f);

        if (paramSeek.load() >= 0.0f && estadoFade == 0) estadoFade = 1;
        actualizarCoefsEQ();

        // 5. Renderizado de audio del sintetizador por sub-bloques exactos a la muestra
        std::size_t muestraOffset = 0;
        double tiempoBloque = tiempoCursor;

        while (muestraOffset < BLOQUE_MUESTRAS) {
            if (estadoFade == 0 || estadoFade == 2) {
                while (indiceNotaActual < notasPista.size() &&
                       notasPista[indiceNotaActual].timestamp <= tiempoBloque) {
                    const Nota& n = notasPista[indiceNotaActual];
                    const float vel = std::clamp(static_cast<float>(n.velocidad) / 127.0f, 0.0f, 1.0f);
                    synth->noteOn(n.pitch, vel, static_cast<float>(n.duracion), 0.0f);
                    ++indiceNotaActual;
                }
            }

            std::size_t muestrasSubBloque = BLOQUE_MUESTRAS - muestraOffset;
            if ((estadoFade == 0 || estadoFade == 2) && indiceNotaActual < notasPista.size()) {
                const double deltaSeg = notasPista[indiceNotaActual].timestamp - tiempoBloque;
                if (deltaSeg > 0.0) {
                    const std::size_t hastaEvento = static_cast<std::size_t>(std::ceil(deltaSeg * SAMPLE_RATE));
                    if (hastaEvento > 0 && hastaEvento < muestrasSubBloque) {
                        muestrasSubBloque = hastaEvento;
                    }
                }
            }

            synth->render(bufferFloat + muestraOffset, muestrasSubBloque);

            muestraOffset += muestrasSubBloque;
            tiempoBloque += static_cast<double>(muestrasSubBloque) / static_cast<double>(SAMPLE_RATE);
        }

        // 6. Cadena comun de amplificador: Overdrive -> EQ -> CabSim -> Chorus -> Gain/Limiter
        for (int m = 0; m < BLOQUE_MUESTRAS; m++) {
            float muestra = bufferFloat[m];

            muestra = aplicarOverdrive(muestra, gainDialActual, odBoostActual, overdriveEstado);
            muestra = aplicarEQ(muestra, eqEstados, eqCoefs);
            muestra = aplicarCabSim(muestra, cabEstados, cabCoefs);

            if (chorusActivoActual) {
                muestra = aplicarAnalogChorus(muestra, bufferChorus, writeIndexChorus,
                                              faseLFO, filtroBBDChorus,
                                              static_cast<float>(SAMPLE_RATE),
                                              rateActual, depthActual);
            } else {
                filtroBBDChorus = muestra;
                guardarBufferChorus(bufferChorus, writeIndexChorus, muestra);
            }

            muestra *= outputLevel;
            const float muestraLimitada = limitarSeñalFinal(muestra);

            // Maquina de estados para salto de tiempo (seek) sin chasquidos
            if (estadoFade == 1) {
                multiplicadorFade -= pasoFade;
                if (multiplicadorFade <= 0.0f) {
                    multiplicadorFade = 0.0f;
                    const float nuevoTiempo = std::max(0.0f, paramSeek.exchange(-1.0f));
                    tiempoCursor = nuevoTiempo;

                    synth->reset();
                    indiceNotaActual = 0;
                    while (indiceNotaActual < notasPista.size() &&
                           notasPista[indiceNotaActual].timestamp <= tiempoCursor) {
                        const Nota& nota = notasPista[indiceNotaActual];
                        const float elapsed = static_cast<float>(tiempoCursor - nota.timestamp);
                        if (elapsed < nota.duracion + 0.4f) {
                            const float vel = std::clamp(static_cast<float>(nota.velocidad) / 127.0f, 0.0f, 1.0f);
                            synth->noteOn(nota.pitch, vel, static_cast<float>(nota.duracion), elapsed);
                        }
                        ++indiceNotaActual;
                    }

                    resetearEstadosDSP();
                    estadoFade = 2;
                }
            } else if (estadoFade == 2) {
                multiplicadorFade += pasoFade;
                if (multiplicadorFade >= 1.0f) {
                    multiplicadorFade = 1.0f;
                    estadoFade = 0;
                }
            }

            bufferFloat[m] = muestraLimitada * multiplicadorFade;
            tiempoCursor += 1.0 / SAMPLE_RATE;
        }

        // 7. Espera de disponibilidad del buffer WaveOut y envio al controlador de audio
        while (reproduciendo.load()) {
            sincronizarPausa();
            if (headers[bufIdx].dwFlags & WHDR_DONE) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!reproduciendo.load()) break;

        for (int m = 0; m < BLOQUE_MUESTRAS; m++) {
            buffersPCM[bufIdx][m] = convertirPCM(bufferFloat[m]);
        }

        if (waveOutWrite(hWaveOut, &headers[bufIdx], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) break;
        bufIdx = (bufIdx + 1) % NUM_BUFFERS;

        tiempoActual.store(static_cast<float>(tiempoCursor));

        // Finalizar reproduccion cuando no queden notas ni voces activas sonando
        if (indiceNotaActual >= notasPista.size() && !synth->tieneVocesActivas() &&
            paramSeek.load() < 0.0f) {
            reproduciendo.store(false);
            break;
        }
    }

    // 8. Liberacion ordenada de recursos WaveOut al detener o finalizar
    if (hWaveOut) {
        waveOutReset(hWaveOut);
        for (int b = 0; b < NUM_BUFFERS; ++b) {
            waveOutUnprepareHeader(hWaveOut, &headers[b], sizeof(WAVEHDR));
        }
        waveOutClose(hWaveOut);
        hWaveOut = nullptr;
    }
    reproduciendo.store(false);
    pausado.store(false);
}