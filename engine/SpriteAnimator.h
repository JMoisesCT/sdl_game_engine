#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

class SpriteRenderer;

// Reproduce animaciones cuadro a cuadro. NO dibuja: calcula el frame actual y se lo
// pasa al SpriteRenderer del mismo objeto (textura + recorte).
//
// Soporta tres formas de organizar los frames:
//  - Hoja unica (addAnimation): todas las animaciones viven en la textura del
//    SpriteRenderer; las celdas se numeran por filas (con 8 columnas, fila 0 = 0..7).
//  - Una tira por archivo (addStripAnimation): cada animacion trae su PROPIA textura,
//    una sola fila horizontal de frames; el animator cambia la textura del
//    SpriteRenderer al activar el clip. Como el AssetManager cachea por ruta, si dos
//    clips usan el mismo archivo comparten textura sin codigo extra.
//  - Una FILA de un sheet (addRowAnimation): la animacion toma una fila concreta de
//    un sheet en grilla (la fila suele ser la direccion en juegos cenitales). Carga
//    la textura del archivo, deduce los frames por fila del ancho y usa las celdas de
//    la fila indicada. addStripAnimation es el caso particular fila = 0.

class SpriteAnimator : public Component {
public:
    // frameWidth/Height = tamano por defecto de cada cuadro (modo hoja unica);
    // sheetColumns = columnas de esa hoja.
    SpriteAnimator(int frameWidth, int frameHeight, int sheetColumns);

    // Modo hoja unica: define una animacion con celdas de la textura del SpriteRenderer.
    void addAnimation(const std::string& name, const std::vector<int>& frames,
                      float fps, bool loop = true);

    // Modo una tira por archivo: carga la textura de esa ruta y DEDUCE la cantidad de
    // frames del ancho (frames = anchoTextura / frameW), asumiendo una sola fila.
    void addStripAnimation(const std::string& name, const std::string& path,
                           int frameW, int frameH, float fps, bool loop = true);

    // Modo una fila de un sheet: carga la textura de esa ruta, deduce los frames por
    // fila del ancho (frames = anchoTextura / frameW) y usa SOLO las celdas de la fila
    // indicada (offset vertical = row * frameH). Util para sheets cenitales donde cada
    // fila es una direccion. Generaliza addStripAnimation (que es row = 0).
    void addRowAnimation(const std::string& name, const std::string& path,
                         int frameW, int frameH, int row, float fps, bool loop = true);

    void play(const std::string& name); // cambia la animacion actual (no reinicia si ya suena)

    void update(float dt) override;

private:
    struct Clip {
        SDL_Texture* texture = nullptr; // nullptr = usa la textura ya puesta en el SpriteRenderer
        int frameW = 0, frameH = 0;     // tamano de cuadro de este clip
        int columns = 0;                // columnas de la hoja de este clip
        std::vector<int> frames;        // celdas que componen la animacion
        float fps = 0.0f;
        bool  loop = true;
    };

    int frameW, frameH, columns; // valores por defecto para addAnimation
    std::unordered_map<std::string, Clip> clips;

    SpriteRenderer* sprite = nullptr; // se resuelve solo en el primer update
    SDL_Texture* appliedTexture = nullptr; // ultima textura puesta en el SpriteRenderer
    std::string current;
    int   currentIndex = 0;
    float timer = 0.0f;

    void applyFrame(); // pasa textura (si toca) y recorte del cuadro actual al SpriteRenderer
};
