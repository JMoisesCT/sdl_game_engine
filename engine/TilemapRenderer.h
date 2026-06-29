#pragma once
#include <string>
#include <vector>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

// Dibuja una grilla de tiles recortados de un tileset, misma idea que el
// setSourceRect del SpriteRenderer pero por celda. El mapa se define en codigo
// con setMap (vector row-major). Convencion del mapa:
//   -1            = celda vacia (no se dibuja ni colisiona)
//   indice >= 0   = celda (0-based) del tileset; col = indice % columnas,
//                   row = indice / columnas.
//
// ANCLAJE: a diferencia de los sprites (cuyo Transform es el CENTRO), aqui el
// Transform del GameObject marca el ORIGEN del mapa: la esquina superior
// izquierda de la celda (0,0). El tamano de cada celda EN EL MUNDO se obtiene
// escalando con el Transform: worldTileW = tileW * scaleX, worldTileH = tileH * scaleY.
//
// COLISION: por cada celda marcada como solida (setSolid) se crea, de forma
// perezosa en el primer update, un GameObject aparte con un BoxCollider estatico
// centrado en esa celda. (No se pueden poner varios BoxCollider en un mismo
// GameObject, por eso van en objetos separados.)

class TilemapRenderer : public Component {
public:
    TilemapRenderer(std::string tilesetPath, int tileW, int tileH, int tilesetColumns);

    // Define el mapa: indices en orden row-major, con su ancho (columnas) y alto (filas).
    void setMap(const std::vector<int>& tiles, int mapWidth, int mapHeight);

    // Marca un indice de tile como solido (genera colision). Se puede llamar varias veces.
    void setSolid(int tileIndex);

    void awake() override;   // carga la textura del tileset
    void update(float dt) override; // build perezoso de los colliders la primera vez
    void render() override;  // dibuja solo las celdas visibles (culling)

private:
    bool isSolid(int tileIndex) const;
    void buildColliders();

    std::string path;
    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)

    int tileW = 0, tileH = 0;       // tamano del tile EN LA IMAGEN (para recortar)
    int tilesetColumns = 0;         // columnas del tileset (para mapear indice -> celda)

    std::vector<int> tiles;         // mapa row-major (-1 vacio)
    int mapWidth = 0, mapHeight = 0;

    std::vector<int> solids;        // indices de tile marcados como solidos
    bool built = false;             // ya se generaron los colliders?
};
