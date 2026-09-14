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
// COLISION: este componente NO colisiona, solo dibuja y responde consultas. Para que
// las celdas marcadas con setSolid frenen a los cuerpos hay que agregar ademas un
// TilemapCollider en el mismo objeto (igual que en Unity: Tilemap + TilemapCollider2D).
// Antes se creaba un GameObject con un BoxCollider POR CELDA solida; se dejo de hacer
// porque con un nivel de verdad son cientos de colliders en una fase O(n^2), y porque
// resolver contra cada tile por separado engancha al personaje en las costuras entre
// tiles vecinos. Ahora la fisica pregunta al mapa (isSolidAt) en vez de instanciar.

class TilemapRenderer : public Component {
public:
    TilemapRenderer() = default; // modo archivo: el tileset lo define loadFromFile
    TilemapRenderer(std::string tilesetPath, int tileW, int tileH, int tilesetColumns);

    // Define el mapa: indices en orden row-major, con su ancho (columnas) y alto (filas).
    void setMap(const std::vector<int>& tiles, int mapWidth, int mapHeight);

    // Carga el mapa COMPLETO desde un archivo de texto (tileset, tile, columns, solid
    // y la grilla). Deja el componente en el mismo estado que el modo en codigo.
    // Devuelve false (y hace SDL_Log) si el archivo falla o la cabecera/grilla es
    // invalida; en ese caso no deja el componente a medio configurar. Ver el .cpp
    // para el formato del archivo.
    bool loadFromFile(const std::string& path);

    // Carga un mapa exportado desde Tiled en formato JSON (capa de tiles + tileset
    // embebido). Deja el componente en el mismo estado que los otros cargadores.
    // Devuelve false (y hace SDL_Log) si falla, sin dejarlo a medio configurar.
    // Ver el .cpp para los supuestos del export y la conversion de indices.
    bool loadFromTiledJson(const std::string& path);

    // Marca un indice de tile como solido (genera colision). Se puede llamar varias veces.
    void setSolid(int tileIndex);

    // --- Consultas del mapa ya cargado ------------------------------------------
    // Valen para cualquier origen (setMap / loadFromFile / loadFromTiledJson). Antes
    // de cargar un mapa devuelven 0. Utiles del lado del juego, p. ej. para centrar
    // la camara segun el ancho del mapa sin cablear esos numeros en el game/.
    int getMapWidth()  const { return mapWidth; }   // ancho en CELDAS (tiles)
    int getMapHeight() const { return mapHeight; }  // alto  en CELDAS (tiles)
    int getTileWidth()  const { return tileW; }     // ancho de un tile EN LA IMAGEN (px)
    int getTileHeight() const { return tileH; }     // alto  de un tile EN LA IMAGEN (px)
    int getTilesetColumns() const { return tilesetColumns; } // columnas del tileset

    // Tamano del mapa COMPLETO en pixeles de MUNDO: las celdas por el tamano de tile
    // y por la escala del Transform del objeto (worldW = mapWidth * tileW * scaleX,
    // worldH = mapHeight * tileH * scaleY). El origen del mapa es el propio Transform,
    // asi que el CENTRO del mapa en x es transform->x + getWorldWidth() * 0.5f.
    float getWorldWidth()  const;
    float getWorldHeight() const;

    // --- Mundo <-> celda ---------------------------------------------------------
    // Tamano de UNA celda en el mundo (el tile de la imagen por la escala del objeto).
    float getTileWorldWidth()  const;
    float getTileWorldHeight() const;
    // Origen del mapa en el mundo = esquina superior izquierda de la celda (0,0).
    float getOriginX() const;
    float getOriginY() const;

    // Punto del mundo -> indices de celda. Puede devolver indices FUERA del mapa
    // (negativos o >= tamano): comprobarlo con isValidCell si hace falta.
    void worldToCell(float worldX, float worldY, int& col, int& row) const;
    // Celda -> CENTRO de esa celda en el mundo.
    void cellToWorld(int col, int row, float& worldX, float& worldY) const;

    bool isValidCell(int col, int row) const;
    int  getTileAt(int col, int row) const;    // indice de tile; -1 si vacia o fuera
    bool isSolidCell(int col, int row) const;  // fuera del mapa = false (no frena)
    bool isSolidAt(float worldX, float worldY) const; // lo mismo, en coords de mundo

    void awake() override;   // carga la textura del tileset
    void render() override;  // dibuja solo las celdas visibles (culling)

private:
    bool isSolid(int tileIndex) const;

    std::string path;
    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)

    int tileW = 0, tileH = 0;       // tamano del tile EN LA IMAGEN (para recortar)
    int tilesetColumns = 0;         // columnas del tileset (para mapear indice -> celda)

    std::vector<int> tiles;         // mapa row-major (-1 vacio)
    int mapWidth = 0, mapHeight = 0;

    std::vector<int> solids;        // indices de tile marcados como solidos
};
