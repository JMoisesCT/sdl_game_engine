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

    void update(float dt) override {
        velocityY += gravity * gravityScale * dt;

        Transform* t = gameObject->transform;
        t->x += velocityX * dt;
        t->y += velocityY * dt;
    }
};
