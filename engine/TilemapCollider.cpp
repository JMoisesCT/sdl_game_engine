#include "TilemapCollider.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "BoxCollider.h"
#include "RigidBody2D.h"
#include "TilemapRenderer.h"

namespace {
    // Margen que se le quita al AABB al buscar que celdas toca. Sin el, un cuerpo
    // apoyado EXACTAMENTE sobre el suelo contaria tambien la celda de al lado (sus
    // bordes coinciden) y se resolverian choques que no existen.
    const float SKIN = 0.01f;
}

void TilemapCollider::awake() {
    gameObject->scene->registerTilemapCollider(this);
}

void TilemapCollider::start() {
    map = gameObject->getComponent<TilemapRenderer>();
    if (!map)
        SDL_Log("TilemapCollider: el objeto %s no tiene TilemapRenderer; no habra colision de tiles.",
                gameObject->name.c_str());
}

// Cuanto mover en X para salir de las celdas solidas que toca el AABB actual.
float TilemapCollider::pushOutX(BoxCollider* box, float dir) const {
    float cw = map->getTileWorldWidth();
    float ch = map->getTileWorldHeight();
    if (cw <= 0.0f || ch <= 0.0f) return 0.0f;

    float left   = box->centerX() - box->halfW();
    float right  = box->centerX() + box->halfW();
    float top    = box->centerY() - box->halfH();
    float bottom = box->centerY() + box->halfH();

    int c0, r0, c1, r1;
    map->worldToCell(left + SKIN, top + SKIN, c0, r0);
    map->worldToCell(right - SKIN, bottom - SKIN, c1, r1);

    float best = 0.0f;
    for (int row = r0; row <= r1; ++row) {
        for (int col = c0; col <= c1; ++col) {
            if (!map->isSolidCell(col, row)) continue;

            float cellLeft  = map->getOriginX() + col * cw;
            float cellRight = cellLeft + cw;

            float push;
            if      (dir > 0.0f) push = cellLeft  - right; // venia hacia la derecha
            else if (dir < 0.0f) push = cellRight - left;  // venia hacia la izquierda
            else {
                // No se movio en X y aun asi solapa (spawn dentro de una pared, mapa
                // movido...): lo sacamos por el lado mas corto para que no se quede
                // atrapado.
                float toLeft  = cellLeft  - right;
                float toRight = cellRight - left;
                push = (std::fabs(toLeft) < std::fabs(toRight)) ? toLeft : toRight;
            }
            if (std::fabs(push) > std::fabs(best)) best = push;
        }
    }
    return best;
}

float TilemapCollider::pushOutY(BoxCollider* box, float dir) const {
    float cw = map->getTileWorldWidth();
    float ch = map->getTileWorldHeight();
    if (cw <= 0.0f || ch <= 0.0f) return 0.0f;

    float left   = box->centerX() - box->halfW();
    float right  = box->centerX() + box->halfW();
    float top    = box->centerY() - box->halfH();
    float bottom = box->centerY() + box->halfH();

    int c0, r0, c1, r1;
    map->worldToCell(left + SKIN, top + SKIN, c0, r0);
    map->worldToCell(right - SKIN, bottom - SKIN, c1, r1);

    float best = 0.0f;
    for (int row = r0; row <= r1; ++row) {
        for (int col = c0; col <= c1; ++col) {
            if (!map->isSolidCell(col, row)) continue;

            float cellTop    = map->getOriginY() + row * ch;
            float cellBottom = cellTop + ch;

            float push;
            if      (dir > 0.0f) push = cellTop    - bottom; // caia: lo subimos
            else if (dir < 0.0f) push = cellBottom - top;    // subia: lo bajamos
            else {
                float toUp   = cellTop    - bottom;
                float toDown = cellBottom - top;
                push = (std::fabs(toUp) < std::fabs(toDown)) ? toUp : toDown;
            }
            if (std::fabs(push) > std::fabs(best)) best = push;
        }
    }
    return best;
}

void TilemapCollider::resolveBody(BoxCollider* box, RigidBody2D* rb) {
    if (!map || !box || !rb) return;

    Transform* t = box->gameObject->transform;
    float newX = t->x, newY = t->y;
    float dx = newX - rb->prevX;   // cuanto se movio este frame en cada eje
    float dy = newY - rb->prevY;

    // --- Pasada X, con la Y del frame ANTERIOR --------------------------------
    // Mirar la X nueva contra la Y vieja es lo que separa de verdad los dos ejes:
    // en esta pasada el cuerpo esta a la altura a la que ya estaba, asi que lo unico
    // que puede encontrarse delante es una pared, nunca el suelo que esta pisando.
    t->y = rb->prevY;
    float pushX = pushOutX(box, dx);
    if (pushX != 0.0f) {
        t->x += pushX;
        rb->velocityX = 0.0f;
    }

    // --- Pasada Y, ya con la X corregida --------------------------------------
    t->y = newY;
    float pushY = pushOutY(box, dy);
    if (pushY != 0.0f) {
        t->y += pushY;
        rb->velocityY = 0.0f;
        if (pushY < 0.0f) rb->grounded = true; // lo empujamos hacia ARRIBA: piso
    }

    // --- Sonda de apoyo --------------------------------------------------------
    // Aunque no haya habido separacion este frame, si hay suelo justo debajo el cuerpo
    // esta apoyado. Sin esto el apoyo dependeria de penetrar el suelo en ESTE frame,
    // que a 600 fps es una centesima de pixel: el grounded parpadearia.
    if (!rb->grounded && groundProbe > 0.0f) {
        float left   = box->centerX() - box->halfW() + SKIN;
        float right  = box->centerX() + box->halfW() - SKIN;
        float feet   = box->centerY() + box->halfH();

        int c0, c1, rowBand, dummy;
        map->worldToCell(left,  feet + groundProbe * 0.5f, c0, rowBand);
        map->worldToCell(right, feet + groundProbe * 0.5f, c1, dummy);
        for (int col = c0; col <= c1; ++col) {
            if (map->isSolidCell(col, rowBand)) { rb->grounded = true; break; }
        }
    }
}
