#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

class SpriteRenderer;

// Orientacion de una "linea" (fila o columna) de un sheet en grilla para
// addLineAnimation: la linea es la animacion y sus frames recorren el eje perpendicular.
enum class StripAxis { Row, Column };

// Reproduce animaciones cuadro a cuadro. NO dibuja: calcula el frame actual y se lo
// pasa al SpriteRenderer del mismo objeto (textura + recorte).
//
// Soporta varias formas de organizar los frames:
//  - Hoja unica (addAnimation): todas las animaciones viven en la textura del
//    SpriteRenderer; las celdas se numeran por filas (con 8 columnas, fila 0 = 0..7).
//  - Una tira por archivo (addStripAnimation): cada animacion trae su PROPIA textura,
//    una sola fila horizontal de frames; el animator cambia la textura del
//    SpriteRenderer al activar el clip. Como el AssetManager cachea por ruta, si dos
//    clips usan el mismo archivo comparten textura sin codigo extra.
//  - Una LINEA de un sheet (addLineAnimation): la animacion es una fila o una columna
//    de un sheet en grilla y sus frames recorren el otro eje. Util para sheets
//    direccionales (cenitales): en unos packs la direccion es la fila y los frames van
//    por columnas; en otros (p.ej. Ninja Adventure) la direccion es la columna y los
//    frames van por filas. addRowAnimation es el atajo del caso fila.

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

    // Modo una linea de un sheet: carga la textura de esa ruta y arma la animacion con
    // una fila o columna (segun 'axis') de un sheet en grilla; los frames recorren el
    // eje perpendicular:
    //   - axis == Row:    la animacion es la fila 'index'; frames = anchoTextura/frameW
    //                     (recorren las columnas); offset vertical del recorte = index*frameH.
    //   - axis == Column: la animacion es la columna 'index'; frames = altoTextura/frameH
    //                     (recorren las filas);  offset horizontal del recorte = index*frameW.
    void addLineAnimation(const std::string& name, const std::string& path,
                          int frameW, int frameH, int index, StripAxis axis,
                          float fps, bool loop = true);

    // Atajo de addLineAnimation con StripAxis::Row (compatibilidad). La animacion es la
    // fila 'row' y sus frames recorren las columnas. addStripAnimation es el caso row = 0.
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
