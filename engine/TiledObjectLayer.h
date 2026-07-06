#pragma once
#include <string>
#include <vector>
#include <map>

// Datos planos de UN objeto de una capa de objetos (objectgroup) de Tiled.
// GENERICO: el motor no sabe que significa cada "type" (no conoce Enemy ni PowerUp);
// solo entrega los datos. La semantica (crear un enemigo, un power-up, una zona...)
// vive del lado del juego, en una fabrica que hace switch sobre "type".
//
// El Transform del motor marca el CENTRO del objeto, por eso cx,cy YA vienen
// convertidos a centro: Tiled entrega la ancla distinta segun el tipo de objeto
// (ver la conversion comentada en el .cpp). Las coordenadas estan en pixeles del
// mapa de Tiled (SIN escalar); el juego las lleva al mundo con el origen y la
// escala que uso su tilemap.
struct TiledObject {
    std::string name;   // "name" del objeto en Tiled (puede ir vacio)
    std::string type;   // "type"/"class" del objeto en Tiled (puede ir vacio)
    float cx = 0.0f;    // centro X en pixeles del mapa de Tiled
    float cy = 0.0f;    // centro Y en pixeles del mapa de Tiled
    float w  = 0.0f;    // ancho en pixeles de Tiled (0 en objetos punto)
    float h  = 0.0f;    // alto  en pixeles de Tiled (0 en objetos punto)
    int   gid = 0;      // >0 si es objeto-tile; 0 si no lo es

    // Propiedades personalizadas de Tiled, separadas por tipo para no depender de
    // std::variant: los textos (string/file/color) van en stringProps; los numeros
    // (int/float) y los bool (como 0.0/1.0) van en numberProps.
    std::map<std::string, std::string> stringProps;
    std::map<std::string, double>      numberProps;

    // Atajos con valor por defecto si la propiedad no existe.
    std::string getString(const std::string& key, const std::string& def = "") const;
    double      getNumber(const std::string& key, double def = 0.0) const;
    bool        hasString(const std::string& key) const;
    bool        hasNumber(const std::string& key) const;
};

// Lee TODAS las capas de objetos (type=="objectgroup") de un mapa de Tiled en JSON
// y devuelve sus objetos como datos planos (centros ya corregidos). Ignora las capas
// de tiles (type=="tilelayer"): esas las dibuja el TilemapRenderer. Ante error de
// apertura o JSON invalido hace SDL_Log y devuelve un vector vacio (no lanza).
// No conoce ningun juego concreto.
std::vector<TiledObject> loadTiledObjectLayers(const std::string& filePath);
