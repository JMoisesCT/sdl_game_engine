#pragma once
#include "Component.h"

// Datos espaciales del objeto. Todo GameObject tiene uno por defecto.
// Es solo datos: no toca SDL.

class Transform : public Component {
public:
    float x = 0.0f;        // posicion en el mundo (pixeles)
    float y = 0.0f;
    float rotation = 0.0f; // grados
    float scaleX = 1.0f;   // 1.0 = tamano original de la imagen
    float scaleY = 1.0f;
};
