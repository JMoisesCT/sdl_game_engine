#include "TiledObjectLayer.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp> // solo aqui: el header queda sin dependencias
#include <fstream>

std::string TiledObject::getString(const std::string& key, const std::string& def) const {
    auto it = stringProps.find(key);
    return it == stringProps.end() ? def : it->second;
}

double TiledObject::getNumber(const std::string& key, double def) const {
    auto it = numberProps.find(key);
    return it == numberProps.end() ? def : it->second;
}

bool TiledObject::hasString(const std::string& key) const {
    return stringProps.find(key) != stringProps.end();
}

bool TiledObject::hasNumber(const std::string& key) const {
    return numberProps.find(key) != numberProps.end();
}

// Lee las properties de un objeto de Tiled. Cada una es {name, type, value}. Los
// tipos numericos (int/float) y el bool (como 0/1) van a numberProps; el resto
// (string/file/color) va a stringProps. Si falta el tipo, se asume string.
static void parseProperties(const nlohmann::json& obj, TiledObject& out) {
    if (!obj.contains("properties") || !obj["properties"].is_array()) return;
    for (const auto& p : obj["properties"]) {
        std::string pname = p.value("name", std::string());
        if (pname.empty() || !p.contains("value")) continue;
        std::string ptype = p.value("type", std::string("string"));
        const auto& v = p["value"];
        if (ptype == "int" || ptype == "float") {
            if (v.is_number()) out.numberProps[pname] = v.get<double>();
        } else if (ptype == "bool") {
            if (v.is_boolean()) out.numberProps[pname] = v.get<bool>() ? 1.0 : 0.0;
        } else {
            if (v.is_string()) out.stringProps[pname] = v.get<std::string>();
        }
    }
}

std::vector<TiledObject> loadTiledObjectLayers(const std::string& filePath) {
    using json = nlohmann::json;
    std::vector<TiledObject> result;

    std::ifstream file(filePath);
    if (!file) {
        SDL_Log("loadTiledObjectLayers: no se pudo abrir '%s'", filePath.c_str());
        return result;
    }

    json j;
    try { file >> j; }
    catch (const std::exception& e) {
        SDL_Log("loadTiledObjectLayers: JSON invalido en '%s': %s", filePath.c_str(), e.what());
        return result;
    }

    if (!j.contains("layers") || !j["layers"].is_array()) {
        SDL_Log("loadTiledObjectLayers: '%s' no tiene layers", filePath.c_str());
        return result;
    }

    for (const auto& layer : j["layers"]) {
        // Solo capas de objetos: las de tiles las maneja el TilemapRenderer.
        if (layer.value("type", std::string()) != "objectgroup") continue;
        if (!layer.contains("objects") || !layer["objects"].is_array()) continue;

        for (const auto& obj : layer["objects"]) {
            TiledObject t;
            t.name = obj.value("name", std::string());
            // Tiled 1.9+ guarda la clase en "class"; versiones previas en "type".
            // Aceptamos ambos para no depender de la version del editor.
            t.type = obj.value("type", std::string());
            if (t.type.empty()) t.type = obj.value("class", std::string());

            float x = (float)obj.value("x", 0.0);
            float y = (float)obj.value("y", 0.0);
            t.w   = (float)obj.value("width", 0.0);
            t.h   = (float)obj.value("height", 0.0);
            t.gid = obj.value("gid", 0);
            bool isPoint = obj.value("point", false);

            // --- Conversion de ORIGEN a CENTRO (el Transform usa centro) --------
            // Tiled entrega la ancla distinta segun el tipo de objeto. La Y de Tiled
            // crece hacia abajo IGUAL que SDL: NO hay flip vertical, solo se corrige
            // el origen. Este es el punto facil de equivocarse; por eso va explicito:
            if (t.gid != 0) {
                // Objeto-tile: (x,y) es la esquina INFERIOR-IZQUIERDA.
                t.cx = x + t.w * 0.5f;
                t.cy = y - t.h * 0.5f;
            } else if (isPoint) {
                // Objeto punto (w=h=0): (x,y) ya ES el centro.
                t.cx = x;
                t.cy = y;
            } else {
                // Rectangulo o elipse: (x,y) es la esquina SUPERIOR-IZQUIERDA.
                t.cx = x + t.w * 0.5f;
                t.cy = y + t.h * 0.5f;
            }

            parseProperties(obj, t);
            result.push_back(std::move(t));
        }
    }

    return result;
}
