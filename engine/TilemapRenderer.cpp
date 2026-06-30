#include "TilemapRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"
#include "BoxCollider.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp> // solo aqui: el header del componente queda sin dependencias
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

TilemapRenderer::TilemapRenderer(std::string tilesetPath, int tileW, int tileH, int tilesetColumns)
    : path(std::move(tilesetPath)), tileW(tileW), tileH(tileH), tilesetColumns(tilesetColumns) {}

void TilemapRenderer::setMap(const std::vector<int>& t, int w, int h) {
    tiles = t;
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

void TilemapRenderer::update(float /*dt*/) {
    // Build perezoso: el alumno llama setMap/setSolid DESPUES de addComponent, asi
    // que en awake el mapa todavia esta vacio. Generamos los colliders la primera
    // vez que corre el update, cuando el mapa ya esta definido.
    if (built) return;
    buildColliders();
    built = true;
}

// Carga un mapa exportado desde Tiled en formato JSON. SUPUESTOS DEL EXPORT:
//  - Mapa ortogonal, tileset EMBEBIDO en el JSON (no externo .tsx).
//  - Se usa la PRIMERA capa de tipo "tilelayer" (si hay varias, se ignora el resto).
//  - Se ignoran los bits de flip/rotacion de Tiled (se enmascaran los 3 bits altos
//    del gid): se asume que el mapa no usa tiles volteados. LIMITACION conocida.
//  - La capa de objetos NO se lee en este paso.
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

    // Primera capa de tiles.
    if (!j.contains("layers") || !j["layers"].is_array()) {
        SDL_Log("TilemapRenderer: '%s' no tiene layers", filePath.c_str());
        return false;
    }
    const json* layer = nullptr;
    for (const auto& l : j["layers"]) {
        if (l.value("type", std::string()) == "tilelayer") { layer = &l; break; }
    }
    if (!layer || !layer->contains("data") || !(*layer)["data"].is_array()) {
        SDL_Log("TilemapRenderer: '%s' no tiene una tilelayer con 'data'",
                filePath.c_str());
        return false;
    }
    const json& data = (*layer)["data"];
    if ((int)data.size() != newWidth * newHeight) {
        SDL_Log("TilemapRenderer: 'data' (%d) no coincide con width*height (%d) en '%s'",
                (int)data.size(), newWidth * newHeight, filePath.c_str());
        return false;
    }

    // Conversion de gids a nuestros indices (0 -> -1; >0 -> gid - firstgid).
    const unsigned FLIP_MASK = 0x1FFFFFFFu; // limpia los 3 bits altos de flip/rotacion
    std::vector<int> newTiles;
    newTiles.reserve(data.size());
    for (const auto& v : data) {
        unsigned gid = v.get<unsigned>() & FLIP_MASK;
        newTiles.push_back(gid == 0 ? -1 : (int)gid - firstgid);
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
    setMap(newTiles, newWidth, newHeight);
    return true;
}

void TilemapRenderer::buildColliders() {
    Transform* t = gameObject->transform;
    float worldTileW = tileW * t->scaleX; // tamano de la celda en el mundo
    float worldTileH = tileH * t->scaleY;

    for (int row = 0; row < mapHeight; ++row) {
        for (int col = 0; col < mapWidth; ++col) {
            int idx = tiles[row * mapWidth + col];
            if (idx < 0) continue;        // vacia
            if (!isSolid(idx)) continue;  // no marcada como solida

            // Un GameObject estatico (sin RigidBody) por celda solida, con el
            // BoxCollider centrado en el centro de la celda (el collider se ancla
            // al CENTRO del Transform).
            GameObject* tileObj = gameObject->scene->createGameObject("TilemapCollider");
            tileObj->transform->x = t->x + col * worldTileW + worldTileW * 0.5f;
            tileObj->transform->y = t->y + row * worldTileH + worldTileH * 0.5f;
            auto bc = tileObj->addComponent<BoxCollider>();
            bc->width = worldTileW;
            bc->height = worldTileH;
        }
    }
}

void TilemapRenderer::render() {
    if (!texture || tiles.empty()) return;

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

    for (int row = rowMin; row <= rowMax; ++row) {
        for (int col = colMin; col <= colMax; ++col) {
            int idx = tiles[row * mapWidth + col];
            if (idx < 0) continue; // celda vacia

            // indice -> celda del tileset -> recorte de la imagen.
            int tsCol = idx % tilesetColumns;
            int tsRow = idx / tilesetColumns;
            SDL_FRect src{
                (float)(tsCol * tileW), (float)(tsRow * tileH),
                (float)tileW, (float)tileH };

            // Esquina superior izquierda de la celda en el mundo -> pantalla.
            float worldX = t->x + col * worldTileW;
            float worldY = t->y + row * worldTileH;
            float screenX, screenY;
            if (cam) cam->worldToScreen(worldX, worldY, screenX, screenY);
            else { screenX = worldX; screenY = worldY; }

            SDL_FRect dst{
                screenX, screenY,
                worldTileW * zoom, worldTileH * zoom };

            SDL_RenderTexture(renderer, texture, &src, &dst);
        }
    }
}
