#include "LevelData.h"
#include "Camera.h"
#include "FollowCamera.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp> // solo aqui: el header queda sin dependencias
#include <algorithm>
#include <fstream>
#include <set>

// FORMATO del archivo .level.json (version 1). Ejemplo:
//
//   {
//     "version": 1,
//     "map": "platformer_level1.json",
//     "nextObjectId": 9,
//     "camera": { "deadZoneWidth": 200, "deadZoneHeight": 200, "lookAhead": 120 },
//     "objects": [
//       { "id": 1, "type": "PlayerStart", "x": 176, "y": 232 },
//       { "id": 2, "type": "Fruit", "x": 200, "y": 242, "properties": { "fruit": "Apple" } },
//       { "id": 3, "type": "TriggerZone", "x": 83, "y": 55, "w": 100, "h": 50 }
//     ]
//   }
//
//  - "map" (obligatorio): el mapa de Tiled, con ruta RELATIVA a la carpeta de este
//    archivo (igual que Tiled guarda la imagen del tileset relativa al mapa).
//  - "camera" (opcional): ajustes de camara; ver applyCameraSettings.
//  - "objects" (opcional): cada objeto lleva "type" y "x","y" = su CENTRO en pixeles del
//    mapa (sin escalar, como en Tiled). "id", "name", "w", "h" y "properties" son
//    opcionales. OJO: a diferencia de Tiled, aqui x,y es SIEMPRE el centro, sea cual sea
//    la forma del objeto; asi coincide con el Transform del motor y no hay conversion.
//  - "properties" es un objeto plano: los textos van a stringProps y los numeros y los
//    bool (como 0/1) a numberProps, la misma convencion que la capa de objetos de Tiled.
//  - Los objetos sin "id" (o con un id repetido) reciben uno libre al cargar, para que
//    cada objeto se pueda identificar siempre.

namespace {
    const int LEVEL_FORMAT_VERSION = 1;

    // Carpeta de un archivo, con la barra final ("assets/maps/nivel.json" -> "assets/maps/").
    std::string folderOf(const std::string& filePath) {
        size_t slash = filePath.find_last_of("/\\");
        return (slash == std::string::npos) ? std::string() : filePath.substr(0, slash + 1);
    }

    // "properties": { "fruit": "Apple", "hp": 3, "boss": true } -> stringProps/numberProps.
    void parseProperties(const nlohmann::json& props, TiledObject& t, const std::string& filePath) {
        for (auto it = props.begin(); it != props.end(); ++it) {
            const auto& v = it.value();
            // El bool va ANTES que el numero: para nlohmann un bool no es un numero, pero
            // asi queda explicito que un bool se guarda como 0/1.
            if      (v.is_string())  t.stringProps[it.key()] = v.get<std::string>();
            else if (v.is_boolean()) t.numberProps[it.key()] = v.get<bool>() ? 1.0 : 0.0;
            else if (v.is_number())  t.numberProps[it.key()] = v.get<double>();
            else SDL_Log("loadLevel: la propiedad '%s' del objeto %d en '%s' no es texto, "
                         "numero ni bool; se ignora.", it.key().c_str(), t.id, filePath.c_str());
        }
    }
}

bool loadLevel(const std::string& filePath, LevelData& out) {
    using json = nlohmann::json;

    std::ifstream file(filePath);
    if (!file) {
        SDL_Log("loadLevel: no se pudo abrir '%s'", filePath.c_str());
        return false;
    }

    json j;
    try { file >> j; }
    catch (const std::exception& e) {
        SDL_Log("loadLevel: JSON invalido en '%s': %s", filePath.c_str(), e.what());
        return false;
    }
    if (!j.is_object()) {
        SDL_Log("loadLevel: '%s' no es un objeto JSON", filePath.c_str());
        return false;
    }

    // Acumulamos en una LOCAL; solo si todo va bien se copia a 'out'.
    LevelData level;
    try {
        int version = j.value("version", LEVEL_FORMAT_VERSION);
        if (version > LEVEL_FORMAT_VERSION)
            SDL_Log("loadLevel: '%s' es de la version %d y este motor lee hasta la %d; "
                    "se intenta leer igual.", filePath.c_str(), version, LEVEL_FORMAT_VERSION);

        if (!j.contains("map") || !j["map"].is_string()) {
            SDL_Log("loadLevel: '%s' no dice que mapa usa (falta \"map\")", filePath.c_str());
            return false;
        }
        level.mapPath = folderOf(filePath) + j["map"].get<std::string>();

        if (j.contains("camera")) {
            const json& cam = j["camera"];
            if (!cam.is_object()) {
                SDL_Log("loadLevel: \"camera\" en '%s' no es un objeto; se ignora.",
                        filePath.c_str());
            } else {
                for (auto it = cam.begin(); it != cam.end(); ++it) {
                    if (it.value().is_number()) level.camera[it.key()] = it.value().get<double>();
                    else SDL_Log("loadLevel: el ajuste de camara '%s' no es un numero; se ignora.",
                                 it.key().c_str());
                }
            }
        }

        if (j.contains("objects")) {
            if (!j["objects"].is_array()) {
                SDL_Log("loadLevel: \"objects\" en '%s' no es una lista", filePath.c_str());
                return false;
            }
            for (const json& o : j["objects"]) {
                if (!o.is_object()) {
                    SDL_Log("loadLevel: hay un objeto que no es {...} en '%s'; se ignora.",
                            filePath.c_str());
                    continue;
                }
                TiledObject t;
                t.id   = o.value("id", 0);
                t.name = o.value("name", std::string());
                t.type = o.value("type", std::string());
                t.cx   = (float)o.value("x", 0.0); // ya es el centro: no hay conversion
                t.cy   = (float)o.value("y", 0.0);
                t.w    = (float)o.value("w", 0.0);
                t.h    = (float)o.value("h", 0.0);
                if (o.contains("properties") && o["properties"].is_object())
                    parseProperties(o["properties"], t, filePath);
                level.objects.push_back(std::move(t));
            }
        }

        // Ids: el siguiente libre es el mayor entre lo que dice el archivo y el mayor id
        // usado + 1. Luego, a los objetos sin id o con uno repetido se les da uno nuevo.
        int maxId = 0;
        for (const TiledObject& t : level.objects) maxId = std::max(maxId, t.id);
        level.nextObjectId = std::max(j.value("nextObjectId", 1), maxId + 1);

        std::set<int> used;
        for (TiledObject& t : level.objects) {
            if (t.id <= 0 || used.count(t.id)) {
                if (t.id > 0)
                    SDL_Log("loadLevel: id %d repetido en '%s'; se le da el %d.",
                            t.id, filePath.c_str(), level.nextObjectId);
                t.id = level.nextObjectId++;
            }
            used.insert(t.id);
        }
    }
    catch (const std::exception& e) {
        // p. ej. "x": "hola" -> value() no puede convertirlo a numero.
        SDL_Log("loadLevel: valor con tipo incorrecto en '%s': %s", filePath.c_str(), e.what());
        return false;
    }

    out = std::move(level);
    return true;
}

void applyCameraSettings(const LevelData& level, Camera* cam, FollowCamera* follow) {
    for (const auto& kv : level.camera) {
        const std::string& key = kv.first;
        float v = (float)kv.second;

        if      (key == "zoom")           { if (cam)    cam->zoom = v; }
        else if (key == "deadZoneWidth")  { if (follow) follow->deadZoneWidth = v; }
        else if (key == "deadZoneHeight") { if (follow) follow->deadZoneHeight = v; }
        else if (key == "smoothSpeed")    { if (follow) follow->smoothSpeed = v; }
        else if (key == "lookAhead")      { if (follow) follow->lookAhead = v; }
        else if (key == "lookAheadSpeed") { if (follow) follow->lookAheadSpeed = v; }
        else SDL_Log("applyCameraSettings: ajuste de camara desconocido '%s'; se ignora.",
                     key.c_str());
    }
}
