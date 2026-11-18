#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include "tipos.h"
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// 1. PRIMERO: La función de limpieza (para que las demás la vean)
inline std::string limpiarCampo(const std::string& s) {
    size_t inicio = s.find_first_not_of(" \t\r\n");
    if (inicio == std::string::npos) return "";
    size_t fin = s.find_last_not_of(" \t\r\n");
    return s.substr(inicio, fin - inicio + 1);
}

// 2. SEGUNDO: La función de parsear técnica
inline Technique parsearTecnica(const std::string& s) {
    std::string t = limpiarCampo(s);
    if (t == "hammer-on" || t == "hammer") return Technique::Hammer;
    if (t == "pull-off"  || t == "pull")   return Technique::Pull;
    if (t == "slide")                      return Technique::Slide;
    if (t == "bend")                       return Technique::Bend;
    if (t == "vibrato")                    return Technique::Vibrato;
    if (t == "normal" || t == "")          return Technique::None;
    return Technique::Unknown;
}

// 3. TERCERO: La función principal que parsea la línea
inline Nota parsearNotaCSV(const std::string& linea) {
    std::stringstream stream(linea);
    std::string campo;
    Nota n;

    // Formato CSV: nota,octava,pitch,duracion,timestamp,velocidad,tecnica,role,parametro
    std::getline(stream, campo, ',');
    n.nota = limpiarCampo(campo);

    std::getline(stream, campo, ',');
    n.octava = std::stoi(limpiarCampo(campo));

    std::getline(stream, campo, ',');
    n.pitch = std::stoi(limpiarCampo(campo));

    std::getline(stream, campo, ',');
    n.duracion = std::stod(limpiarCampo(campo));

    std::getline(stream, campo, ',');
    n.timestamp = std::stod(limpiarCampo(campo));

    std::getline(stream, campo, ',');
    n.velocidad = std::stoi(limpiarCampo(campo));

    std::getline(stream, campo, ',');
    n.tecnica = limpiarCampo(campo);
    n.tech = parsearTecnica(n.tecnica);

    std::getline(stream, campo, ',');
    n.role = static_cast<Role>(std::stoi(limpiarCampo(campo)));

    std::getline(stream, campo, ',');
    n.parametro = std::stof(limpiarCampo(campo));

    return n;
}

// Declaración de la función que va en el .cpp
std::vector<Nota> parsearCSV(const std::string& rutaArchivo);

#endif // CSV_PARSER_H