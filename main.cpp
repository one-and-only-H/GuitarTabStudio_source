// ─────────────────────────────────────────────────────────────
// INTERRUPTOR DE MODO:
// 1 = Ejecuta el test de consola (útil para probar el mapper)
// 0 = Ejecuta la ventana normal de Qt
#define MODO_TEST_CONSOLA 1
// ─────────────────────────────────────────────────────────────

#include <iostream>

#if MODO_TEST_CONSOLA

// --- INCLUDES PARA EL TEST DE CONSOLA ---
#include "csv_parser.h"
#include "mapper/mapper.h" // Ajustá la ruta según tu estructura de carpetas
// INCLUIMOS LAS IMPLEMENTACIONES DIRECTAMENTE PARA QUE EL LINKER LAS ENCUENTRE
#include "csv_parser.cpp"
#include "acordes.cpp"
#include "mapper/mapper.cpp"
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>
#include <filesystem> // Para manejar rutas y carpetas
#include <cstdlib>    // Para system()

namespace fs = std::filesystem;

// --- FUNCIÓN AUXILIAR PARA IMPRIMIR LA TABLATURA ---
void imprimirTablaturaManual(const Mapper::ResultadoMapeo& resultado) {
    if (resultado.eventos.empty()) {
        std::cout << "No hay eventos para mostrar.\n";
        return;
    }

    std::vector<double> columnas;
    for (const auto& ev : resultado.eventos) {
        if (columnas.empty() || std::abs(ev.timestamp - columnas.back()) > 0.01) {
            columnas.push_back(ev.timestamp);
        }
    }

    std::vector<std::string> lineas(6);
    const char* nombres[] = {"e", "B", "G", "D", "A", "E"};
    for (int i = 0; i < 6; i++) {
        lineas[i] = std::string(nombres[i]) + "|";
    }
    std::string lineaAcordes = "  ";

    for (double t : columnas) {
        std::string notasCuerda[6] = {"--", "--", "--", "--", "--", "--"};
        std::string acordeStr = " ";

        for (const auto& ev : resultado.eventos) {
            if (std::abs(ev.timestamp - t) < 0.01) {
                if (ev.acorde.has_value()) {
                    acordeStr = ev.acorde->nombreAcorde;
                }
                for (const auto& nota : ev.notas) {
                    std::string trasteStr = (nota.traste >= 10)
                    ? std::to_string(nota.traste)
                    : "-" + std::to_string(nota.traste);
                    notasCuerda[nota.cuerda - 1] = trasteStr;
                }
            }
        }

        for (int i = 0; i < 6; i++) {
            lineas[i] += notasCuerda[i] + "-";
        }
        lineaAcordes += acordeStr;
        for (int i = acordeStr.length(); i < 3; i++) lineaAcordes += " ";
    }

    std::cout << "\n--- TABLATURA GENERADA ---\n\n";
    for (int i = 0; i < 6; i++) {
        std::cout << lineas[i] << "\n";
    }
    std::cout << lineaAcordes << "\n\n";
}

    // --- MAIN DEL TEST DE CONSOLA ---
    int main(int argc, char* argv[]) {
        // Usamos rutas absolutas para evitar problemas con el directorio de ejecución (build/)
        std::string archivoMIDI = "C:/Users/SANDRA/Documents/guitarTabCreator/Audio files/Weezer_Island_In_The_Sun (1).mid";
        std::string carpetaSongs = "C:/Users/SANDRA/Documents/guitarTabCreator/GuitarTabCreator/songs";
        std::string scriptPython = "C:/Users/SANDRA/Documents/guitarTabCreator/GuitarTabCreator/scripts/midi2csv.py";

        // 1. Chequear que los archivos existan antes de ejecutar
        if (!fs::exists(archivoMIDI)) {
            std::cerr << "ERROR: No se encuentra el archivo MIDI en:\n" << archivoMIDI << "\n";
            return 1;
        }
        if (!fs::exists(scriptPython)) {
            std::cerr << "ERROR: No se encuentra el script Python en:\n" << scriptPython << "\n";
            return 1;
        }

        fs::create_directories(carpetaSongs);

        // 2. Ejecutar el script de Python
        std::cout << "1. Ejecutando script de Python...\n";
        
        // En Windows, a veces "python" no está en el PATH pero "py" sí.
        // El comando queda: py "ruta/script.py" "ruta/archivo.mid"
        std::string comando = "python \"" + scriptPython + "\" \"" + archivoMIDI + "\"";
        
        std::cout << "Ejecutando: " << comando << "\n";
        int resultadoPython = std::system(comando.c_str());

        if (resultadoPython != 0) {
            std::cerr << "Error al ejecutar el script de Python (Codigo: " << resultadoPython << ").\n";
            std::cerr << "Verifica que Python esté instalado y que 'pretty_midi' esté disponible.\n";
            return 1;
        }


        // 3. Buscar los archivos CSV generados (automático)
        // Extraemos el nombre base del MIDI (sin ruta ni extensión)
        fs::path pathMidi(archivoMIDI);
        std::string nombreBase = pathMidi.stem().string();
        std::string prefijo = nombreBase + "_";
        
        std::vector<std::string> archivosCSV;
        
        for (const auto& entry : fs::directory_iterator(carpetaSongs)) {
            std::string nombre = entry.path().filename().string();
            if (nombre.find(prefijo) == 0 && entry.path().extension() == ".csv") {
                archivosCSV.push_back(entry.path().string());
            }
        }

        // ... (el resto del código sigue igual)

    // 4. Procesar cada CSV generado
    for (const std::string& rutaCSV : archivosCSV) {
        std::cout << "\n============================================\n";
        std::cout << "PROCESANDO: " << rutaCSV << "\n";
        std::cout << "============================================\n";

        try {
            std::vector<Nota> notas = parsearCSV(rutaCSV);

            if (notas.empty()) {
                std::cout << "El CSV está vacío. Saltando...\n";
                continue;
            }
            std::cout << "Notas cargadas: " << notas.size() << "\n";

            Mapper::ResultadoMapeo resultado = Mapper::mapear(notas);

            std::cout << "Eventos generados: " << resultado.eventos.size() << "\n";

            if (!resultado.warnings.empty()) {
                std::cout << "Advertencias (" << resultado.warnings.size() << "):\n";
                for (const auto& w : resultado.warnings) {
                    std::cout << "  [WARN] " << w.mensaje << " en t=" << w.timestamp << "\n";
                }
            }

            imprimirTablaturaManual(resultado);

        } catch (const std::exception& e) {
            std::cerr << "[EXCEPCIÓN] " << e.what() << "\n";
        }
    }

    return 0;
}

#else

// --- MAIN ORIGINAL DE QT ---
#include "mainwindow.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "GuitarTabCreator_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}

#endif