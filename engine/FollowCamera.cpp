#include "FollowCamera.h"
#include "GameObject.h"
#include "Transform.h"

void FollowCamera::update(float dt) {
    if (!target) return;

    Transform* cam = gameObject->transform;
    Transform* tgt = target->transform;

    float halfW = deadZoneWidth  * 0.5f;
    float halfH = deadZoneHeight * 0.5f;

    // Por defecto la camara se queda donde esta...
    float desiredX = cam->x;
    float desiredY = cam->y;

    // ...y solo se reubica si el objetivo cruza el borde de la zona muerta,
    // lo justo para dejarlo de nuevo sobre ese borde.
    float dx = tgt->x - cam->x;
    if      (dx >  halfW) desiredX = tgt->x - halfW;
    else if (dx < -halfW) desiredX = tgt->x + halfW;

    float dy = tgt->y - cam->y;
    if      (dy >  halfH) desiredY = tgt->y - halfH;
    else if (dy < -halfH) desiredY = tgt->y + halfH;

    if (smoothSpeed <= 0.0f) {
        cam->x = desiredX; // seguimiento duro
        cam->y = desiredY;
    } else {
        float k = smoothSpeed * dt; // interpolacion sencilla hacia el destino
        if (k > 1.0f) k = 1.0f;
        cam->x += (desiredX - cam->x) * k;
        cam->y += (desiredY - cam->y) * k;
    }
}
