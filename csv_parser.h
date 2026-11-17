#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include "tipos.h"
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

#include<algorithm>

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

inline Nota parsearNotaCSV(const std::string& linea){
    std::stringstream stream(linea);
    std::string campo;
    Nota n;
    
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
    n.tech = parsearTecnica(n.tecnica); // Convertir al indice del enum

    std::getline(stream, campo, ',');
    n.role = static_cast<Role>(std::stoi(limpiarCampo(campo))); 

    std::getline(stream, campo, ',');
    n.parametro = std::stof(limpiarCampo(campo));

    return n;
}

inline std::string limpiarCampo(const std::string& s) { //por si acaso hay espacios al inicio o final de un campo, los eliminamos
    size_t inicio = s.find_first_not_of(" \t\r\n");

    if (inicio == std::string::npos)
        return "";

    size_t fin = s.find_last_not_of(" \t\r\n");

    return s.substr(inicio, fin - inicio + 1);
}


/*no hay tanto manejo de error pq ya el script de python se encarga de filtrar y limpiar el csv, 
pero si hay algun error en la lectura del csv se lanza una excepcion std::runtime_error*/

// Lanza std::runtime_error si el archivo no se puede abrir
std::vector<Nota> parsearCSV(const std::string& rutaArchivo);

#endif // CSV_PARSER_H