#pragma once
#include <string>
#include "Component.h"

struct SDL_Texture; // declaraciones adelantadas: SDL / SDL3_ttf solo en el .cpp
struct SDL_Renderer;
struct TTF_Font;

// Color RGBA propio del motor para no arrastrar SDL_Color al header (SDL fuera de
// los headers de composicion). El .cpp lo traduce a SDL_Color.
struct TextColor {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

// Dibuja una cadena de texto con una fuente cacheada. NO maneja logica (puntaje,
// mensajes): solo pinta el string que se le da.
//
// screenSpace = true  -> dibuja en coordenadas de PANTALLA directas, IGNORANDO la
//   camara (HUD: no scrollea con el fondo). El Transform marca el CENTRO del texto.
// screenSpace = false -> el Transform es una posicion de MUNDO y pasa por la camara.
//
// Nitidez de pixel art: usa TTF_RenderText_Solid (sin antialiasing) y dibuja en
// coordenadas enteras. Regenerar la textura del texto es caro, asi que solo se
// regenera cuando cambia el texto, el color o la fuente (dirty flag).

class TextRenderer : public Component {
public:
    // La fuente la presta el AssetManager (no somos dueno). Se asigna con setFont
    // (p. ej. scene.getAssets().loadFont(ruta, tamanio)).
    void setFont(TTF_Font* f);

    // Cambia el texto a mostrar. Si es igual al actual, no hace nada (no ensucia).
    void setText(const std::string& t);
    const std::string& getText() const { return text; }

    // Cambia el color del texto.
    void setColor(TextColor c);

    bool screenSpace = true; // true = HUD (ignora camara); false = texto en el mundo

    void render() override;
    ~TextRenderer() override;

private:
    void rebuildTexture(SDL_Renderer* renderer); // regenera 'texture' desde el string
    void releaseTexture();                        // libera la textura actual (si hay)

    std::string text;
    TTF_Font*   font  = nullptr;      // prestada por el AssetManager (no somos dueno)
    TextColor   color;

    SDL_Texture* texture = nullptr;   // SI somos dueno: la generamos nosotros
    float texW = 0.0f;                // tamanio de la textura del texto (px)
    float texH = 0.0f;

    bool dirty = true; // hay que (re)generar la textura antes de dibujar
};
