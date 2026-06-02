#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "Component.h"

class SpriteRenderer;

// Reproduce animaciones desde un spritesheet. NO dibuja: calcula el frame actual
// y se lo pasa al SpriteRenderer del mismo objeto como recorte (setSourceRect).
// Las celdas se numeran por filas: con 8 columnas, fila 0 = 0..7, fila 1 = 8..15...

class SpriteAnimator : public Component {
public:
    // frameWidth/Height = tamano de cada cuadro; sheetColumns = columnas de la hoja.
    SpriteAnimator(int frameWidth, int frameHeight, int sheetColumns);

    // Define una animacion: nombre, celdas que la componen, cuadros por segundo, si repite.
    void addAnimation(const std::string& name, const std::vector<int>& frames,
                      float fps, bool loop = true);

    void play(const std::string& name); // cambia la animacion actual (no reinicia si ya suena)

    void update(float dt) override;

private:
    struct Clip {
        std::vector<int> frames;
        float fps;
        bool  loop;
    };

    int frameW, frameH, columns;
    std::unordered_map<std::string, Clip> clips;

    SpriteRenderer* sprite = nullptr; // se resuelve solo en el primer update
    std::string current;
    int   currentIndex = 0;
    float timer = 0.0f;

    void applyFrame(); // traduce la celda actual a un recorte en el SpriteRenderer
};
