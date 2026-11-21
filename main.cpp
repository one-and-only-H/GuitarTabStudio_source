// ─────────────────────────────────────────────────────────────
// INTERRUPTOR DE MODO:
// 0 = Ejecuta la ventana principal de Qt (GuitarTabStudio 1.0)
// 1 = Ejecuta el test de consola (diagnóstico rápido del pipeline)
#define MODO_TEST_CONSOLA 0
// ─────────────────────────────────────────────────────────────

#if MODO_TEST_CONSOLA

#include "csv_parser.h"
#include "mapper/mapper.h"
#include "tab_generator.h"

#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::string archivoMIDI = "C:/Users/SANDRA/Documents/guitarTabCreator/Audio files/test_boulevard_of_broken_dreams.mid";
    std::string carpetaSongs = "C:/Users/SANDRA/Documents/guitarTabCreator/GuitarTabCreator/songs";
    std::string scriptPython = "C:/Users/SANDRA/Documents/guitarTabCreator/GuitarTabCreator/scripts/midi2csv.py";

    if (!fs::exists(archivoMIDI)) {
        std::cerr << "ERROR: No se encuentra el archivo MIDI en:\n" << archivoMIDI << "\n";
        return 1;
    }
    if (!fs::exists(scriptPython)) {
        std::cerr << "ERROR: No se encuentra el script Python en:\n" << scriptPython << "\n";
        return 1;
    }

    fs::create_directories(carpetaSongs);

    std::cout << "1. Ejecutando script de Python...\n";
    std::string comando = "python \"" + scriptPython + "\" \"" + archivoMIDI + "\"";
    std::cout << "Ejecutando: " << comando << "\n";

    int resultadoPython = std::system(comando.c_str());
    if (resultadoPython != 0) {
        std::cerr << "Error al ejecutar el script de Python (Codigo: " << resultadoPython << ").\n";
        return 1;
    }

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

            Mapper::SongContext contexto = Mapper::construirContexto(notas);
            std::cout << "\n=== CONTEXTO TEMPORAL ===\n";
            for (const auto& perfil : contexto.contexto.perfiles) {
                std::cout
                    << "Bloque "
                    << std::fixed << std::setprecision(2)
                    << perfil.inicio << " - " << perfil.fin
                    << " s | Mediana MIDI: "
                    << perfil.registroMediano
                    << '\n';
            }

            Mapper::ResultadoMapeo resultado = Mapper::mapear(notas);
            std::cout << "Eventos generados: " << resultado.eventos.size() << "\n";

            if (!resultado.warnings.empty()) {
                std::cout << "Advertencias (" << resultado.warnings.size() << "):\n";
                for (const auto& w : resultado.warnings) {
                    std::cout << "  [WARN] " << w.mensaje << " en t=" << w.timestamp << "\n";
                }
            }

            // Generar y mostrar con el nuevo TabGenerator (8 filas + sostenidos + silencios)
            TabGenerator::ConfigTab cfg;
            cfg.ancho_maximo_linea = 100;

            std::string rutaSalidaTxt = fs::path(rutaCSV).replace_extension(".txt").string();
            std::string tablatura;

            if (TabGenerator::procesarYExportar(resultado, rutaSalidaTxt, &tablatura, cfg)) {
                std::cout << "\n" << tablatura << "\n";
                std::cout << "[OK] Exportado en: " << rutaSalidaTxt << "\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "[EXCEPCIÓN] " << e.what() << "\n";
        }
    }

    std::cout << "\nPresiona Enter para cerrar...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    return 0;
}

#else

// --- MODO VENTANA PRINCIPAL QT ---
#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("GuitarTabStudio");
    a.setApplicationVersion("1.0");
    a.setStyle(QStyleFactory::create("Fusion"));

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