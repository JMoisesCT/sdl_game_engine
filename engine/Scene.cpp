#include "Scene.h"
#include "BoxCollider.h"
#include "RigidBody2D.h"
#include "GameObject.h"
#include "Transform.h"

#include <cmath>

void Scene::resolveCollisions() {
    // 1) Reiniciar el estado "apoyado" de los cuerpos dinamicos.
    for (BoxCollider* c : colliders) {
        if (RigidBody2D* rb = c->gameObject->getComponent<RigidBody2D>())
            rb->grounded = false;
    }

    // 2) Revisar cada par de colliders (fuerza bruta, O(n^2)).
    for (size_t i = 0; i < colliders.size(); ++i) {
        for (size_t j = i + 1; j < colliders.size(); ++j) {
            BoxCollider* a = colliders[i];
            BoxCollider* b = colliders[j];

            float dx = b->centerX() - a->centerX();
            float dy = b->centerY() - a->centerY();
            float px = (a->halfW() + b->halfW()) - std::fabs(dx); // penetracion en X
            float py = (a->halfH() + b->halfH()) - std::fabs(dy); // penetracion en Y

            if (px <= 0.0f || py <= 0.0f) continue; // no se tocan

            // Hay solapamiento: avisar a ambos objetos.
            a->gameObject->notifyCollision(b->gameObject);
            b->gameObject->notifyCollision(a->gameObject);

            // Los triggers solo avisan, no separan.
            if (a->isTrigger || b->isTrigger) continue;

            RigidBody2D* arb = a->gameObject->getComponent<RigidBody2D>();
            RigidBody2D* brb = b->gameObject->getComponent<RigidBody2D>();
            bool aDyn = (arb != nullptr);
            bool bDyn = (brb != nullptr);
            if (!aDyn && !bDyn) continue; // dos estaticos: nada que mover

            // Cuanto le toca moverse a cada uno.
            float aShare = aDyn ? (bDyn ? 0.5f : 1.0f) : 0.0f;
            float bShare = bDyn ? (aDyn ? 0.5f : 1.0f) : 0.0f;

            if (px < py) {
                // Separar por el eje de MENOR penetracion (aqui, X).
                float dirA = (dx > 0.0f) ? -1.0f : 1.0f; // 'a' se aleja de 'b'
                a->gameObject->transform->x += dirA * px * aShare;
                b->gameObject->transform->x -= dirA * px * bShare;
                if (arb) arb->velocityX = 0.0f;
                if (brb) brb->velocityX = 0.0f;
            } else {
                // Separar por el eje Y.
                float dirA = (dy > 0.0f) ? -1.0f : 1.0f;
                a->gameObject->transform->y += dirA * py * aShare;
                b->gameObject->transform->y -= dirA * py * bShare;
                if (arb) arb->velocityY = 0.0f;
                if (brb) brb->velocityY = 0.0f;

                // El que queda ENCIMA queda apoyado (sirve para saltar).
                if (dy > 0.0f) { if (arb) arb->grounded = true; }
                else           { if (brb) brb->grounded = true; }
            }
        }
    }
}

#include <algorithm>

void Scene::removeDeadObjects() {
    // 1) Quitar del registro los colliders de objetos muertos (antes de borrarlos).
    colliders.erase(
        std::remove_if(colliders.begin(), colliders.end(),
            [](BoxCollider* c){ return !c->gameObject->alive; }),
        colliders.end());

    // 2) Eliminar los objetos muertos (esto libera sus componentes).
    objects.erase(
        std::remove_if(objects.begin(), objects.end(),
            [](const std::unique_ptr<GameObject>& o){ return !o->alive; }),
        objects.end());
}
