#include "TilemapRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"
#include "BoxCollider.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

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
    texture = gameObject->scene->getAssets().loadTexture(path);
}

void TilemapRenderer::update(float /*dt*/) {
    // Build perezoso: el alumno llama setMap/setSolid DESPUES de addComponent, asi
    // que en awake el mapa todavia esta vacio. Generamos los colliders la primera
    // vez que corre el update, cuando el mapa ya esta definido.
    if (built) return;
    buildColliders();
    built = true;
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
