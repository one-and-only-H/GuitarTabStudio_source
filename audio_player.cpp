#include "audio_player.h"

#include <limits>

namespace {

BiquadCoef normalizarCoeficientes(float b0, float b1, float b2,
                                  float a0, float a1, float a2) {
    if (std::abs(a0) < 1.0e-12f) return BiquadCoef{};
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

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

void reconstruirEnvolvente(EnvolventeADSR& env, float elapsed,
                           float duracion, float ataque, float decayRate,
                           float sustain, float release) {
    env = EnvolventeADSR{};
    const float attackTime = std::max(0.001f, ataque);
    const float decayTime = std::max(0.001f, 0.5f * decayRate);

    if (elapsed < attackTime && elapsed < duracion) {
        env.estado = EstadoADSR::ATTACK;
        env.tiempoEnEstado = elapsed;
        env.valorActual = std::clamp(elapsed / attackTime, 0.0f, 1.0f);
        return;
    }

    float valorAlSoltar = 1.0f;
    if (duracion < attackTime) {
        valorAlSoltar = std::clamp(duracion / attackTime, 0.0f, 1.0f);
    } else if (duracion < attackTime + decayTime) {
        const float progreso = (duracion - attackTime) / decayTime;
        valorAlSoltar = 1.0f - ((1.0f - sustain) * std::clamp(progreso, 0.0f, 1.0f));
    } else {
        valorAlSoltar = sustain;
    }

    if (elapsed < duracion) {
        if (elapsed < attackTime + decayTime) {
            env.estado = EstadoADSR::DECAY;
            env.tiempoEnEstado = elapsed - attackTime;
            const float progreso = std::clamp(env.tiempoEnEstado / decayTime, 0.0f, 1.0f);
            env.valorActual = 1.0f - ((1.0f - sustain) * progreso);
        } else {
            env.estado = EstadoADSR::SUSTAIN;
            env.tiempoEnEstado = elapsed - attackTime - decayTime;
            env.valorActual = sustain;
        }
        return;
    }

    const float tiempoRelease = std::max(0.001f, release);
    const float releaseElapsed = elapsed - duracion;
    if (releaseElapsed >= tiempoRelease || valorAlSoltar <= 0.0f) {
        env.estado = EstadoADSR::OFF;
        env.valorActual = 0.0f;
        env.valorInicioRelease = 0.0f;
    } else {
        env.estado = EstadoADSR::RELEASE;
        env.tiempoEnEstado = releaseElapsed;
        env.valorInicioRelease = valorAlSoltar;
        env.valorActual = valorAlSoltar * (1.0f - releaseElapsed / tiempoRelease);
    }
}

NotaSonando crearNotaSonando(const Nota& nota, TipoPastilla pastilla, float elapsed) {
    NotaSonando ns{};
    ns.pitchOriginal = nota.pitch;
    ns.timestampInicio = static_cast<float>(nota.timestamp);
    ns.duracionTotal = static_cast<float>(nota.duracion);
    ns.tiempoTranscurrido = elapsed;

    const float velocidad = std::clamp(static_cast<float>(nota.velocidad) / 127.0f, 0.0f, 1.0f);
    ns.volumenVelocity = velocidad * velocidad;

    const float frecuenciaBase = obtenerFrecuencia(nota.pitch);
    const auto armonicos = dividirEnArmonicos(frecuenciaBase);
    ns.perfil = obtenerPerfil(pastilla, TipoAtaque::LET_RING);

    for (int h = 0; h < NUM_HARMONICS; h++) {
        ns.frecuencias[h] = armonicos[h];
        ns.fases[h] = std::fmod(2.0f * static_cast<float>(PI) * armonicos[h] * elapsed,
                               2.0f * static_cast<float>(PI));

        reconstruirEnvolvente(ns.envolventes[h], elapsed, ns.duracionTotal,
                              ns.perfil.ataques[h], ns.perfil.tasasDecaimiento[h],
                              ns.perfil.sustentos[h], ns.perfil.releases[h]);
    }
    return ns;
}

bool notaSigueSonando(const NotaSonando& ns) {
    for (int h = 0; h < NUM_HARMONICS; h++) {
        if (ns.envolventes[h].estado != EstadoADSR::OFF &&
            ns.envolventes[h].valorActual > 0.003f) {
            return true;
        }
    }
    return false;
}
} // namespace


//yo tengo cara de querer pelearme con la api de windows? 


AudioPlayer::AudioPlayer() {
    actualizarCoefsEQ();
}

AudioPlayer::~AudioPlayer() {
    stop();
}
//al menos esta parte la supe hacer
bool AudioPlayer::prepararPista(const std::vector<Nota>& notas, uint32_t sampleRate) {
    if (reproduciendo.load()) return false;
    if (sampleRate != SAMPLE_RATE) return false;

    if (hiloAudio.joinable()) hiloAudio.join();

    notasPista = notas;
    std::sort(notasPista.begin(), notasPista.end(),
              [](const Nota& a, const Nota& b) { return a.timestamp < b.timestamp; });

    eqEstados = EQEstados{};
    writeIndexChorus = 0;
    faseLFO = 0.0f;
    std::fill(std::begin(bufferChorus), std::end(bufferChorus), 0.0f);
    tiempoActual.store(0.0f);
    paramSeek.store(-1.0f);
    return true;
}

//c ya later
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
//llamar de nuevo a lo del .h
void AudioPlayer::actualizarCoefsEQ() {
    const float bass = std::clamp(paramBass.load(), EQ_GAIN_MIN, EQ_GAIN_MAX);
    const float mid = std::clamp(paramMid.load(), EQ_GAIN_MIN, EQ_GAIN_MAX);
    const float treble = std::clamp(paramTreble.load(), EQ_GAIN_MIN, EQ_GAIN_MAX);

    eqCoefs.bass = lowShelf(250.0f, bass, static_cast<float>(SAMPLE_RATE));
    eqCoefs.mid = peakingEQ(1000.0f, mid, 0.707f, static_cast<float>(SAMPLE_RATE));
    eqCoefs.treble = highShelf(4000.0f, treble, static_cast<float>(SAMPLE_RATE));
}

void AudioPlayer::bucleReproduccion() {
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

    // Doble buffer (ping-pong) para evitar cortes/pops entre bloques
    constexpr int NUM_BUFFERS = 3;
    std::vector<int16_t> buffersPCM[NUM_BUFFERS];
    WAVEHDR headers[NUM_BUFFERS]{};

    for (int b = 0; b < NUM_BUFFERS; ++b) {
        buffersPCM[b].resize(BLOQUE_MUESTRAS, 0);
        headers[b].lpData = reinterpret_cast<LPSTR>(buffersPCM[b].data());
        headers[b].dwBufferLength = static_cast<DWORD>(BLOQUE_MUESTRAS * sizeof(int16_t));

        if (waveOutPrepareHeader(hWaveOut, &headers[b], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            for (int j = 0; j < b; ++j) waveOutUnprepareHeader(hWaveOut, &headers[j], sizeof(WAVEHDR));
            waveOutClose(hWaveOut);
            hWaveOut = nullptr;
            reproduciendo.store(false);
            return;
        }
        headers[b].dwFlags |= WHDR_DONE;
    }

    int bufIdx = 0;
    double tiempoCursor = 0.0;
    size_t indiceNotaActual = 0;
    std::vector<NotaSonando> notasActivas;
    notasActivas.reserve(notasPista.size());
    float bufferFloat[BLOQUE_MUESTRAS]{};

    int estadoFade = 0; // 0 = normal, 1 = fade out, 2 = fade in
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

        const float gainActual = paramGain.load();
        const float driveActual = paramODDrive.load();
        const bool odActivoActual = odActivo.load();
        const bool chorusActivoActual = chorusActivo.load();
        const TipoPastilla pastillaActual = static_cast<TipoPastilla>(paramTipoPastilla.load());
        const float rateActual = std::clamp(paramCRate.load(), CHORUS_RATE_MIN, CHORUS_RATE_MAX);
        const float depthActual = std::clamp(paramCDepth.load(), 0.0f, 1.0f);

        if (paramSeek.load() >= 0.0f && estadoFade == 0) estadoFade = 1;
        actualizarCoefsEQ();

        for (int m = 0; m < BLOQUE_MUESTRAS; m++) {
            const double tiempoMuestra = tiempoCursor;

            if (estadoFade == 0 || estadoFade == 2) {
                while (indiceNotaActual < notasPista.size() &&
                       notasPista[indiceNotaActual].timestamp <= tiempoMuestra) {
                    if (notasActivas.size() >= 18) { // (ahora pongo más explciación de por qué cortar notas sobre el límite ideal)
                        notasActivas.erase(notasActivas.begin()); //sobre lo de arriba, es que me está consumiendo 20% de la cpu, lanzar eso sería un crimen
                    }
                    notasActivas.push_back(crearNotaSonando(notasPista[indiceNotaActual], pastillaActual, 0.0f));
                    ++indiceNotaActual;
                }
            }

            float sumaNotas = 0.0f;
            for (auto& ns : notasActivas) {
                float presamples[NUM_HARMONICS]{};
                generarPreSamples(ns.frecuencias, ns.fases, presamples, static_cast<float>(SAMPLE_RATE));
                aplicarEnvolventes(presamples, ns.envolventes, ns.perfil,
                                   ns.duracionTotal, ns.tiempoTranscurrido,
                                   static_cast<float>(SAMPLE_RATE));
                float muestraNota = mezclarArmonicos(presamples, ns.perfil.pesos);
                muestraNota *= ns.volumenVelocity;
                sumaNotas += muestraNota;
                ns.tiempoTranscurrido += 1.0f / SAMPLE_RATE;
            }

            const int cantidadNotas = std::max(1, static_cast<int>(notasActivas.size()));
            float muestra = normalizar(sumaNotas, cantidadNotas);

            muestra = aplicarEQ(muestra, eqEstados, eqCoefs);
            muestra = aplicarOverdrive(muestra, driveActual, odActivoActual);
            //un pequeño filtro para simular cómo se comen los agudos los parlantes -by herre
            static float filtroCabina = 0.0f;
                if (odActivoActual) {
                    filtroCabina += 0.38f * (muestra - filtroCabina);
                    muestra = filtroCabina;
                } else {
                    filtroCabina = muestra;
                }

            if (chorusActivoActual) {
                muestra = aplicarChorus(muestra, bufferChorus, writeIndexChorus,
                                         faseLFO, static_cast<float>(SAMPLE_RATE),
                                         rateActual, depthActual);
            } else {
                guardarBufferChorus(bufferChorus, writeIndexChorus, muestra);
            }
            muestra *= gainActual;
            const float muestraLimitada = limitarSeñalFinal(muestra);

            if (estadoFade == 1) {
                multiplicadorFade -= pasoFade;
                if (multiplicadorFade <= 0.0f) {
                    multiplicadorFade = 0.0f;
                    const float nuevoTiempo = std::max(0.0f, paramSeek.exchange(-1.0f));
                    tiempoCursor = nuevoTiempo;

                    notasActivas.clear();
                    indiceNotaActual = 0;
                    while (indiceNotaActual < notasPista.size() &&
                           notasPista[indiceNotaActual].timestamp <= tiempoCursor) {
                        const Nota& nota = notasPista[indiceNotaActual];
                        const float elapsed = static_cast<float>(tiempoCursor - nota.timestamp);
                        NotaSonando reconstruida = crearNotaSonando(nota, pastillaActual, elapsed);
                        if (notaSigueSonando(reconstruida)) notasActivas.push_back(reconstruida);
                        ++indiceNotaActual;
                    }

                    writeIndexChorus = 0;
                    faseLFO = 0.0f;
                    std::fill(std::begin(bufferChorus), std::end(bufferChorus), 0.0f);
                    eqEstados = EQEstados{};
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

        // Esperar solo si ambos buffers siguen en cola
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

        notasActivas.erase(
            std::remove_if(notasActivas.begin(), notasActivas.end(),
                           [](const NotaSonando& ns) { return !notaSigueSonando(ns); }),
            notasActivas.end()
        );

        if (indiceNotaActual >= notasPista.size() && notasActivas.empty() &&
            paramSeek.load() < 0.0f) {
            reproduciendo.store(false);
            break;
        }
    }

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