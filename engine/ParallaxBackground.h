#pragma once
#include <string>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

// Fondo que se repite para llenar la pantalla y que se mueve MENOS que el mundo, para
// dar sensacion de profundidad (parallax). La imagen debe ser TILEABLE (los fondos de
// Pixel Adventure son de 64x64 y encajan consigo mismos).
//
// FACTOR: cuanto acompana a la camara.
//   0.0 = no se mueve nunca (como si estuviera infinitamente lejos)
//   0.3 = se mueve un poco: montanas del fondo
//   1.0 = se mueve igual que el mundo (ya no hay efecto de profundidad)
// Varias capas con factores distintos (0.2, 0.5, 0.8) dan el efecto completo.
//
// Se dibuja en coordenadas de PANTALLA y llena todo el viewport, asi que no depende de
// donde este su Transform. Para que quede detras de todo, hay que ponerle al GameObject
// un sortingOrder bajo (en los ejemplos, -100).

class ParallaxBackground : public Component {
public:
    explicit ParallaxBackground(std::string imagePath);

    float factorX = 0.3f;  // 0 = quieto, 1 = se mueve con el mundo
    float factorY = 0.3f;
    float scale   = 1.0f;  // escala del mosaico (4.0 con tiles de 64 px = 256 px)

    // Desplazamiento automatico en pixeles por segundo, para fondos que van solos
    // (nubes, agua). 0 = quieto.
    float scrollSpeedX = 0.0f;
    float scrollSpeedY = 0.0f;

    void awake() override;
    void update(float dt) override;
    void render() override;

private:
    std::string path;
    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)
    float texW = 0.0f, texH = 0.0f;
    float autoX = 0.0f, autoY = 0.0f; // desplazamiento acumulado del scroll automatico
};
