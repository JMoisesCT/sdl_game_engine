#include "Respawn.h"

#include "GameObject.h"
#include "Transform.h"
#include "RigidBody2D.h"
#include "Health.h"

void Respawn::start() {
    rb     = gameObject->getComponent<RigidBody2D>();
    health = gameObject->getComponent<Health>();

    // Punto por defecto: donde empieza el nivel.
    pointX = gameObject->transform->x;
    pointY = gameObject->transform->y;
}

void Respawn::setPoint(float x, float y) {
    pointX = x;
    pointY = y;
}

void Respawn::die() {
    if (dying) return; // ya estaba muriendo: no reiniciar la cuenta
    dying = true;
    timer = delay;
}

void Respawn::respawnNow() {
    Transform* t = gameObject->transform;
    t->x = pointX;
    t->y = pointY;

    if (rb) {
        rb->velocityX = 0.0f;
        rb->velocityY = 0.0f;
        // La posicion anterior tambien: si no, la colision contra el tilemap creeria
        // que el objeto hizo un viaje enorme en un frame y lo separaria mal.
        rb->prevX = t->x;
        rb->prevY = t->y;
        rb->grounded = false;
    }

    if (health) health->resetHealth();

    dying = false;
    timer = 0.0f;
    if (onRespawn) onRespawn();
}

void Respawn::update(float dt) {
    if (!dying) return;

    // Congelar al muerto mientras corre la cuenta atras.
    if (rb) { rb->velocityX = 0.0f; rb->velocityY = 0.0f; }

    timer -= dt;
    if (timer <= 0.0f) respawnNow();
}
