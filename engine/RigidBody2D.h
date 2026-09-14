#pragma once
#include "GameObject.h"
#include "Transform.h"

// Cuerpo fisico simple: integra velocidad y gravedad para mover el Transform.
// Tener un RigidBody2D vuelve al objeto "dinamico"; sin el, es estatico (pared, suelo).

class RigidBody2D : public Component {
public:
    float velocityX = 0.0f;     // px/seg
    float velocityY = 0.0f;
    float gravity = 980.0f;     // px/seg^2 (hacia abajo)
    float gravityScale = 1.0f;  // 0 = sin gravedad (juegos top-down)
    bool  grounded = false;     // true si esta apoyado sobre algo (lo setea la fisica)

    // Posicion que tenia ANTES de integrar este frame. La usa la colision contra el
    // tilemap para separar por ejes (saber con que X/Y venia y hacia donde se movia);
    // tambien la necesitaran las plataformas atravesables de abajo a arriba.
    // La escribe este componente en cada update: no tocarla desde fuera.
    float prevX = 0.0f;
    float prevY = 0.0f;

    void update(float dt) override {
        Transform* t = gameObject->transform;
        prevX = t->x;  // antes de moverse: de aqui venia
        prevY = t->y;

        velocityY += gravity * gravityScale * dt;

        t->x += velocityX * dt;
        t->y += velocityY * dt;
    }
};
