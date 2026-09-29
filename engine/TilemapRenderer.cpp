#include "TilemapRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp> // solo aqui: el header del componente queda sin dependencias
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace {
    // Una tilelayer del JSON de Tiled junto con lo que hereda de sus grupos padre.
    struct TiledTileLayerRef {
        const nlohmann::json* json;
        bool  visible;
        float opacity;
    };

    // Junta las tilelayer en orden de dibujo. El array "layers" de Tiled ya viene de
    // abajo hacia arriba (la primera es la del fondo). Los grupos ("group") solo
    // organizan: se aplanan, y su visibilidad y opacidad se heredan a sus capas.
    void collectTileLayers(const nlohmann::json& list, bool visible, float opacity,
                           std::vector<TiledTileLayerRef>& out) {
        for (const auto& l : list) {
            std::string type = l.value("type", std::string());
            bool  v = visible && l.value("visible", true);
            float o = opacity * (float)l.value("opacity", 1.0);
            if (type == "tilelayer")
                out.push_back({ &l, v, o });
            else if (type == "group" && l.contains("layers") && l["layers"].is_array())
                collectTileLayers(l["layers"], v, o, out);
        }
    }

    // Propiedad booleana personalizada de una capa de Tiled; 'def' si no la tiene.
    bool layerBoolProperty(const nlohmann::json& layer, const std::string& name, bool def) {
        if (!layer.contains("properties") || !layer["properties"].is_array()) return def;
        for (const auto& p : layer["properties"]) {
            if (p.value("name", std::string()) == name &&
                p.contains("value") && p["value"].is_boolean())
                return p["value"].get<bool>();
        }
        return def;
    }
}

TilemapRenderer::TilemapRenderer(std::string tilesetPath, int tileW, int tileH, int tilesetColumns)
    : path(std::move(tilesetPath)), tileW(tileW), tileH(tileH), tilesetColumns(tilesetColumns) {}

void TilemapRenderer::setMap(const std::vector<int>& t, int w, int h) {
    // El mapa en codigo es de UNA capa: reemplaza las que hubiera.
    Layer layer;
    layer.tiles = t;
    layers.clear();
    layers.push_back(std::move(layer));
    mapWidth = w;
    mapHeight = h;
}

void TilemapRenderer::setSolid(int tileIndex) {
    if (!isSolid(tileIndex)) solids.push_back(tileIndex);
}

bool TilemapRenderer::isSolid(int tileIndex) const {
    return std::find(solids.begin(), solids.end(), tileIndex) != solids.end();
}

void TilemapRenderer::awake() {
    // Sin tileset todavia (modo archivo: lo pondra loadFromFile despues del addComponent).
    if (path.empty()) return;
    texture = gameObject->scene->getAssets().loadTexture(path);
}

// Carga el mapa completo desde un archivo de texto. Formato:
//   tileset <ruta de la imagen>   (la ruta puede tener espacios: el resto de la linea)
//   tile <w> <h>                  (tamano del tile en la imagen)
//   columns <n>                   (columnas del tileset)
//   solid <i> <i> ...             (indices solidos; opcional, puede repetirse)
//   luego la grilla: una fila por linea, indices separados por coma, -1 = vacio.
// Lineas vacias y las que empiezan con # se ignoran. Devuelve false ante cualquier
// error (y hace SDL_Log) sin dejar el componente a medio configurar.
bool TilemapRenderer::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) {
        SDL_Log("TilemapRenderer: no se pudo abrir el archivo '%s'", filePath.c_str());
        return false;
    }

    // Acumulamos en LOCALES; solo al final, si todo fue bien, tocamos el estado real.
    std::string tilesetPath;
    int newTileW = 0, newTileH = 0, newColumns = 0;
    bool haveTileset = false, haveTile = false, haveColumns = false;
    std::vector<int> newSolids;
    std::vector<int> newTiles;
    int newWidth = -1, newHeight = 0;

    std::string line;
    int lineNo = 0;
    while (std::getline(file, line)) {
        ++lineNo;

        // Trim de espacios a ambos lados.
        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;        // linea vacia
        size_t b = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(a, b - a + 1);
        if (trimmed[0] == '#') continue;             // comentario

        // Primera palabra = directiva de cabecera o inicio de la grilla.
        std::istringstream ss(trimmed);
        std::string head;
        ss >> head;

        if (head == "tileset") {
            // El resto de la linea es la ruta (puede tener espacios).
            std::string rest;
            std::getline(ss, rest);
            size_t ra = rest.find_first_not_of(" \t");
            if (ra == std::string::npos) {
                SDL_Log("TilemapRenderer: 'tileset' sin ruta (linea %d)", lineNo);
                return false;
            }
            tilesetPath = rest.substr(ra);
            haveTileset = true;
        }
        else if (head == "tile") {
            if (!(ss >> newTileW >> newTileH)) {
                SDL_Log("TilemapRenderer: 'tile' espera <w> <h> (linea %d)", lineNo);
                return false;
            }
            haveTile = true;
        }
        else if (head == "columns") {
            if (!(ss >> newColumns)) {
                SDL_Log("TilemapRenderer: 'columns' espera <n> (linea %d)", lineNo);
                return false;
            }
            haveColumns = true;
        }
        else if (head == "solid") {
            int idx;
            while (ss >> idx) newSolids.push_back(idx); // puede haber varios por linea
        }
        else {
            // No es directiva conocida: es una fila de la grilla (indices con coma).
            std::vector<int> row;
            std::stringstream rowSS(trimmed);
            std::string cell;
            while (std::getline(rowSS, cell, ',')) {
                size_t ca = cell.find_first_not_of(" \t\r\n");
                if (ca == std::string::npos) {
                    SDL_Log("TilemapRenderer: celda vacia en la grilla (linea %d)", lineNo);
                    return false;
                }
                size_t cb = cell.find_last_not_of(" \t\r\n");
                std::string num = cell.substr(ca, cb - ca + 1);
                try {
                    row.push_back(std::stoi(num));
                } catch (...) {
                    SDL_Log("TilemapRenderer: valor invalido '%s' en la grilla (linea %d)",
                            num.c_str(), lineNo);
                    return false;
                }
            }
            if (newWidth < 0) newWidth = (int)row.size(); // ancho = columnas de la 1a fila
            else if ((int)row.size() != newWidth) {
                SDL_Log("TilemapRenderer: la fila %d tiene %d columnas (se esperaban %d)",
                        lineNo, (int)row.size(), newWidth);
                return false;
            }
            newTiles.insert(newTiles.end(), row.begin(), row.end());
            ++newHeight;
        }
    }

    // Validacion de cabecera y de que exista grilla.
    if (!haveTileset || !haveTile || !haveColumns) {
        SDL_Log("TilemapRenderer: cabecera incompleta en '%s' (faltan tileset/tile/columns)",
                filePath.c_str());
        return false;
    }
    if (newWidth <= 0 || newHeight <= 0) {
        SDL_Log("TilemapRenderer: el archivo '%s' no contiene grilla", filePath.c_str());
        return false;
    }

    // Todo OK: ahora si volcamos al estado real, igual que el modo en codigo.
    path = tilesetPath;
    tileW = newTileW;
    tileH = newTileH;
    tilesetColumns = newColumns;
    texture = gameObject->scene->getAssets().loadTexture(path); // awake ya corrio: cargar aqui
    solids.clear();
    for (int s : newSolids) setSolid(s);
    setMap(newTiles, newWidth, newHeight);
    return true;
}

// --- Consultas de mundo <-> celda -------------------------------------------------
// Son la base de la colision: el TilemapCollider y la fase de fisica preguntan al mapa
// en vez de instanciar un collider por celda.

float TilemapRenderer::getTileWorldWidth()  const { return tileW * gameObject->transform->scaleX; }
float TilemapRenderer::getTileWorldHeight() const { return tileH * gameObject->transform->scaleY; }
float TilemapRenderer::getOriginX() const { return gameObject->transform->x; }
float TilemapRenderer::getOriginY() const { return gameObject->transform->y; }

void TilemapRenderer::mapToWorld(float mapX, float mapY, float& worldX, float& worldY) const {
    // Un pixel del mapa mide en el mundo lo que la escala del objeto.
    worldX = getOriginX() + mapX * gameObject->transform->scaleX;
    worldY = getOriginY() + mapY * gameObject->transform->scaleY;
}

void TilemapRenderer::worldToMap(float worldX, float worldY, float& mapX, float& mapY) const {
    float sx = gameObject->transform->scaleX, sy = gameObject->transform->scaleY;
    mapX = (sx != 0.0f) ? (worldX - getOriginX()) / sx : 0.0f;
    mapY = (sy != 0.0f) ? (worldY - getOriginY()) / sy : 0.0f;
}

void TilemapRenderer::worldToCell(float worldX, float worldY, int& col, int& row) const {
    float cw = getTileWorldWidth(), ch = getTileWorldHeight();
    if (cw <= 0.0f || ch <= 0.0f) { col = row = -1; return; }
    // floor y no truncado: a la izquierda del origen los indices deben salir negativos.
    col = (int)std::floor((worldX - getOriginX()) / cw);
    row = (int)std::floor((worldY - getOriginY()) / ch);
}

void TilemapRenderer::cellToWorld(int col, int row, float& worldX, float& worldY) const {
    float cw = getTileWorldWidth(), ch = getTileWorldHeight();
    worldX = getOriginX() + col * cw + cw * 0.5f; // centro de la celda
    worldY = getOriginY() + row * ch + ch * 0.5f;
}

bool TilemapRenderer::isValidCell(int col, int row) const {
    return col >= 0 && row >= 0 && col < mapWidth && row < mapHeight;
}

int TilemapRenderer::getTileAt(int col, int row, int layer) const {
    if (!isValidCell(col, row) || layer < 0 || layer >= (int)layers.size()) return -1;
    return layers[layer].tiles[(size_t)row * mapWidth + col];
}

bool TilemapRenderer::isSolidCell(int col, int row) const {
    // Fuera del mapa NO es solido: el personaje puede salirse por los lados o caer por
    // abajo. Si un juego quiere paredes invisibles en el borde, las pone el juego.
    if (!isValidCell(col, row)) return false;
    size_t cell = (size_t)row * mapWidth + col;
    for (const Layer& layer : layers) {
        // Las capas decorativas no cuentan. La visibilidad NO importa: una capa oculta
        // con colision sirve como "capa de colision invisible" pintada a mano.
        if (!layer.collision) continue;
        int idx = layer.tiles[cell];
        if (idx >= 0 && isSolid(idx)) return true;
    }
    return false;
}

bool TilemapRenderer::isSolidAt(float worldX, float worldY) const {
    int col, row;
    worldToCell(worldX, worldY, col, row);
    return isSolidCell(col, row);
}

// Carga un mapa exportado desde Tiled en formato JSON. SUPUESTOS DEL EXPORT:
//  - Mapa ortogonal, tileset EMBEBIDO en el JSON (no externo .tsx).
//  - Se leen TODAS las capas de tipo "tilelayer", en el orden de Tiled (la de abajo
//    del panel es la del fondo). Los grupos de capas se aplanan. Cada capa respeta su
//    "visible" y su "opacity", y su propiedad booleana personalizada "collision"
//    (si falta vale true; false = capa decorativa que nunca frena). El offset y el
//    parallax por capa de Tiled se ignoran.
//  - Capas en CSV y mapa finito: cada capa trae 'data' como array de width*height.
//  - Se ignoran los bits de flip/rotacion de Tiled (se enmascaran los 3 bits altos
//    del gid): se asume que el mapa no usa tiles volteados. LIMITACION conocida.
//  - Las capas de objetos NO se leen aqui (ver TiledObjectLayer / LevelData).
// CONVERSION DE INDICES: Tiled usa 0 = vacio y los tiles empiezan en "firstgid";
// nuestro motor usa -1 = vacio y 0 = primer tile. Por cada gid: 0 -> -1, y >0 ->
// (gid sin bits de flip) - firstgid. SOLIDEZ: por cada tile del tileset con una
// propiedad booleana name=="solid" y value==true, se llama a setSolid(id).
bool TilemapRenderer::loadFromTiledJson(const std::string& filePath) {
    using json = nlohmann::json;

    std::ifstream file(filePath);
    if (!file) {
        SDL_Log("TilemapRenderer: no se pudo abrir '%s'", filePath.c_str());
        return false;
    }

    json j;
    try { file >> j; }
    catch (const std::exception& e) {
        SDL_Log("TilemapRenderer: JSON invalido en '%s': %s", filePath.c_str(), e.what());
        return false;
    }

    // Dimensiones del mapa (en tiles) y tamano de cada tile (en la imagen).
    if (!j.contains("width") || !j.contains("height") ||
        !j.contains("tilewidth") || !j.contains("tileheight")) {
        SDL_Log("TilemapRenderer: faltan width/height/tilewidth/tileheight en '%s'",
                filePath.c_str());
        return false;
    }
    int newWidth = j["width"].get<int>();
    int newHeight = j["height"].get<int>();
    int newTileW = j["tilewidth"].get<int>();
    int newTileH = j["tileheight"].get<int>();

    // Tileset embebido (el primero del array).
    if (!j.contains("tilesets") || !j["tilesets"].is_array() || j["tilesets"].empty()) {
        SDL_Log("TilemapRenderer: '%s' no tiene tilesets", filePath.c_str());
        return false;
    }
    const json& ts = j["tilesets"][0];
    if (ts.contains("source")) {
        SDL_Log("TilemapRenderer: tileset externo (.tsx) no soportado en '%s'",
                filePath.c_str());
        return false;
    }
    if (!ts.contains("image") || !ts.contains("columns")) {
        SDL_Log("TilemapRenderer: el tileset de '%s' no trae image/columns",
                filePath.c_str());
        return false;
    }
    std::string image = ts["image"].get<std::string>();
    int newColumns = ts["columns"].get<int>();
    int firstgid = ts.value("firstgid", 1); // normalmente 1; no lo hardcodeamos

    // La ruta de la imagen viene relativa al archivo del mapa: la resolvemos
    // respecto a la carpeta del .json para pasarla al AssetManager.
    std::string dir;
    size_t slash = filePath.find_last_of("/\\");
    if (slash != std::string::npos) dir = filePath.substr(0, slash + 1);
    std::string imagePath = dir + image;

    // Todas las capas de tiles, en orden de dibujo.
    if (!j.contains("layers") || !j["layers"].is_array()) {
        SDL_Log("TilemapRenderer: '%s' no tiene layers", filePath.c_str());
        return false;
    }
    std::vector<TiledTileLayerRef> refs;
    collectTileLayers(j["layers"], true, 1.0f, refs);
    if (refs.empty()) {
        SDL_Log("TilemapRenderer: '%s' no tiene ninguna tilelayer", filePath.c_str());
        return false;
    }

    // Conversion de gids a nuestros indices (0 -> -1; >0 -> gid - firstgid).
    const unsigned FLIP_MASK = 0x1FFFFFFFu; // limpia los 3 bits altos de flip/rotacion
    std::vector<Layer> newLayers;
    for (const TiledTileLayerRef& ref : refs) {
        const json& l = *ref.json;
        std::string layerName = l.value("name", std::string());
        if (!l.contains("data") || !l["data"].is_array()) {
            SDL_Log("TilemapRenderer: la capa '%s' de '%s' no trae 'data' como array "
                    "(exportala en CSV y con el mapa finito)",
                    layerName.c_str(), filePath.c_str());
            return false;
        }
        const json& data = l["data"];
        if ((int)data.size() != newWidth * newHeight) {
            SDL_Log("TilemapRenderer: la capa '%s' trae %d celdas y el mapa tiene %d en '%s'",
                    layerName.c_str(), (int)data.size(), newWidth * newHeight,
                    filePath.c_str());
            return false;
        }

        Layer layer;
        layer.name      = layerName;
        layer.visible   = ref.visible;
        layer.opacity   = ref.opacity;
        layer.collision = layerBoolProperty(l, "collision", true);
        layer.tiles.reserve(data.size());
        for (const auto& v : data) {
            unsigned gid = v.get<unsigned>() & FLIP_MASK;
            layer.tiles.push_back(gid == 0 ? -1 : (int)gid - firstgid);
        }
        newLayers.push_back(std::move(layer));
    }

    // Tiles solidos: propiedad booleana "solid"==true en el tileset embebido.
    std::vector<int> newSolids;
    if (ts.contains("tiles") && ts["tiles"].is_array()) {
        for (const auto& tdef : ts["tiles"]) {
            if (!tdef.contains("id") || !tdef.contains("properties")) continue;
            int id = tdef["id"].get<int>();
            for (const auto& p : tdef["properties"]) {
                if (p.value("name", std::string()) == "solid" &&
                    p.contains("value") && p["value"].is_boolean() &&
                    p["value"].get<bool>()) {
                    newSolids.push_back(id);
                }
            }
        }
    }

    // Todo OK: recien ahora tocamos el estado real (igual que los otros cargadores).
    path = imagePath;
    tileW = newTileW;
    tileH = newTileH;
    tilesetColumns = newColumns;
    texture = gameObject->scene->getAssets().loadTexture(path); // awake ya corrio
    solids.clear();
    for (int s : newSolids) setSolid(s);
    layers = std::move(newLayers);
    mapWidth = newWidth;
    mapHeight = newHeight;
    return true;
}

float TilemapRenderer::getWorldWidth() const {
    // Ancho en celdas * tamano de tile en la imagen * escala del objeto en el mundo.
    return mapWidth * tileW * gameObject->transform->scaleX;
}

float TilemapRenderer::getWorldHeight() const {
    return mapHeight * tileH * gameObject->transform->scaleY;
}

void TilemapRenderer::render() {
    if (!texture || layers.empty()) return;

    SDL_Renderer* renderer = gameObject->scene->getRenderer();
    Transform* t = gameObject->transform;
    Camera* cam = gameObject->scene->getActiveCamera();

    float worldTileW = tileW * t->scaleX;
    float worldTileH = tileH * t->scaleY;
    if (worldTileW <= 0.0f || worldTileH <= 0.0f) return;

    float zoom = cam ? cam->getZoom() : 1.0f;

    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &outW, &outH);

    // Rectangulo de MUNDO que se ve en pantalla, para dibujar solo las celdas
    // visibles (culling). Con camara, la vista esta centrada en su Transform y
    // escalada por el zoom; sin camara, pantalla == mundo.
    float viewLeft, viewTop, viewRight, viewBottom;
    if (cam) {
        float camX = cam->gameObject->transform->x;
        float camY = cam->gameObject->transform->y;
        viewLeft = camX - (outW * 0.5f) / zoom;
        viewRight = camX + (outW * 0.5f) / zoom;
        viewTop = camY - (outH * 0.5f) / zoom;
        viewBottom = camY + (outH * 0.5f) / zoom;
    } else {
        viewLeft = 0.0f; viewRight = (float)outW;
        viewTop = 0.0f;  viewBottom = (float)outH;
    }

    // Bordes de mundo -> indices de columna/fila respecto al origen del mapa,
    // con clamp a los limites del mapa.
    int colMin = (int)std::floor((viewLeft - t->x) / worldTileW);
    int colMax = (int)std::floor((viewRight - t->x) / worldTileW);
    int rowMin = (int)std::floor((viewTop - t->y) / worldTileH);
    int rowMax = (int)std::floor((viewBottom - t->y) / worldTileH);
    colMin = std::max(colMin, 0);
    rowMin = std::max(rowMin, 0);
    colMax = std::min(colMax, mapWidth - 1);
    rowMax = std::min(rowMax, mapHeight - 1);

    // La textura del tileset es PRESTADA y otros la pueden estar usando (el shooter
    // dibuja sus balas con ella): si una capa cambia la opacidad, se restaura al final.
    float baseAlpha = 1.0f;
    SDL_GetTextureAlphaModFloat(texture, &baseAlpha);

    // Una pasada completa por capa, de la del fondo a la de delante: cada capa tapa
    // a la anterior, igual que en Tiled.
    for (const Layer& layer : layers) {
        if (!layer.visible || layer.opacity <= 0.0f) continue;
        SDL_SetTextureAlphaModFloat(texture, baseAlpha * layer.opacity);

        for (int row = rowMin; row <= rowMax; ++row) {
            for (int col = colMin; col <= colMax; ++col) {
                int idx = layer.tiles[row * mapWidth + col];
                if (idx < 0) continue; // celda vacia

                // indice -> celda del tileset -> recorte de la imagen.
                int tsCol = idx % tilesetColumns;
                int tsRow = idx / tilesetColumns;
                SDL_FRect src{
                    (float)(tsCol * tileW), (float)(tsRow * tileH),
                    (float)tileW, (float)tileH };

                // Esquinas de la celda en el MUNDO: la izq/arriba de esta celda y la
                // izq/arriba de la SIGUIENTE (que es su der/abajo). Pasamos ambas a
                // pantalla y redondeamos cada borde a entero. Asi el borde derecho de
                // un tile cae en el mismo entero que el borde izquierdo del vecino:
                // sin costura sub-pixel ni solape (la fuente del bleeding al escalar
                // con coordenadas fraccionarias por camara/zoom).
                float worldLeft = t->x + col * worldTileW;
                float worldTop = t->y + row * worldTileH;
                float worldRight = worldLeft + worldTileW;
                float worldBottom = worldTop + worldTileH;

                float sLeft, sTop, sRight, sBottom;
                if (cam) {
                    cam->worldToScreen(worldLeft, worldTop, sLeft, sTop);
                    cam->worldToScreen(worldRight, worldBottom, sRight, sBottom);
                } else {
                    sLeft = worldLeft; sTop = worldTop;
                    sRight = worldRight; sBottom = worldBottom;
                }

                float dLeft = std::round(sLeft);
                float dTop = std::round(sTop);
                SDL_FRect dst{
                    dLeft, dTop,
                    std::round(sRight) - dLeft,
                    std::round(sBottom) - dTop };

                SDL_RenderTexture(renderer, texture, &src, &dst);
            }
        }
    }

    SDL_SetTextureAlphaModFloat(texture, baseAlpha);
}
