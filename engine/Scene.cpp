#include "Scene.h"
#include "BoxCollider.h"
#include "RigidBody2D.h"
#include "GameObject.h"
#include "Transform.h"
#include "TilemapCollider.h"

#include <algorithm>
#include <cmath>

// La fase de fisica son DOS pasadas, en este orden:
//   1) Tilemap: los cuerpos se separan de la geometria del nivel, eje por eje.
//   2) Pares:   collider contra collider (AABB), para objeto contra objeto.
// El 'grounded' se reinicia UNA sola vez al principio, porque las dos pasadas pueden
// darlo por bueno (el suelo puede ser un tile o una plataforma que sea un objeto).
void Scene::resolveCollisions() {
    for (BoxCollider* c : colliders) {
        if (RigidBody2D* rb = c->gameObject->getComponent<RigidBody2D>())
            rb->grounded = false;
    }

    resolveTilemapCollisions();
    resolvePairCollisions();
}

void Scene::resolveTilemapCollisions() {
    if (tilemaps.empty()) return;

    for (BoxCollider* c : colliders) {
        if (c->isTrigger) continue; // los triggers atraviesan el nivel

        // Solo los cuerpos DINAMICOS se separan del mapa: sin RigidBody2D no hay
        // movimiento propio ni posicion anterior contra la que comparar.
        RigidBody2D* rb = c->gameObject->getComponent<RigidBody2D>();
        if (!rb) continue;

        for (TilemapCollider* tc : tilemaps)
            tc->resolveBody(c, rb);
    }
}

void Scene::resolvePairCollisions() {
    // Revisar cada par de colliders (fuerza bruta, O(n^2)).
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

void Scene::render() {
    // Orden de dibujo por capas. Antes se dibujaba en orden de CREACION, asi que un
    // objeto creado despues tapaba a uno creado antes aunque fuera el fondo.
    // stable_sort: a igual sortingOrder se conserva el orden de creacion.
    drawList.clear();
    drawList.reserve(objects.size());
    for (auto& o : objects) drawList.push_back(o.get());

    std::stable_sort(drawList.begin(), drawList.end(),
        [](const GameObject* a, const GameObject* b) {
            return a->sortingOrder < b->sortingOrder;
        });

    for (GameObject* o : drawList) o->render();
}

void Scene::removeDeadObjects() {
    // 1) Quitar del registro los colliders de objetos muertos (antes de borrarlos).
    colliders.erase(
        std::remove_if(colliders.begin(), colliders.end(),
            [](BoxCollider* c){ return !c->gameObject->alive; }),
        colliders.end());

    // 2) Lo mismo con los tilemaps solidos.
    tilemaps.erase(
        std::remove_if(tilemaps.begin(), tilemaps.end(),
            [](TilemapCollider* t){ return !t->gameObject->alive; }),
        tilemaps.end());

    // 3) Eliminar los objetos muertos (esto libera sus componentes).
    objects.erase(
        std::remove_if(objects.begin(), objects.end(),
            [](const std::unique_ptr<GameObject>& o){ return !o->alive; }),
        objects.end());
}
