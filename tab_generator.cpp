#include "tab_generator.h"

#include <iostream>

namespace TabGenerator {

bool procesarYExportar(
    const Mapper::ResultadoMapeo& resultado,
    const std::string& rutaArchivoSalida,
    std::string* salidaTextoOut,
    const ConfigTab& config
) {
    std::string tablatura = generarTablatura(resultado, config);

    if (salidaTextoOut != nullptr) {
        *salidaTextoOut = tablatura;
    }

    if (!rutaArchivoSalida.empty()) {
        if (!exportarTablaturaTxt(resultado, rutaArchivoSalida, config)) {
            std::cerr << "[TabGenerator] Error: no se pudo escribir el archivo '"
                      << rutaArchivoSalida << "'.\n";
            return false;
        }
    }

    return true;
}

bool procesarDesdeNotas(
    const std::vector<Nota>& notasRaw,
    const std::string& rutaArchivoSalida,
    std::string* salidaTextoOut,
    const ConfigTab& config
) {
    Mapper::ResultadoMapeo resultado = Mapper::mapear(notasRaw);

    return procesarYExportar(
        resultado,
        rutaArchivoSalida,
        salidaTextoOut,
        config
    );
}

} // namespace TabGenerator