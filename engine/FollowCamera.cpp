#include "FollowCamera.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"
#include "TilemapRenderer.h"

#include <SDL3/SDL.h>

void FollowCamera::setBounds(float minX, float minY, float maxX, float maxY) {
    boundsMinX = minX; boundsMinY = minY;
    boundsMaxX = maxX; boundsMaxY = maxY;
    hasBounds = true;
}

void FollowCamera::setBoundsFromTilemap(const TilemapRenderer* map) {
    if (!map) return;
    // El Transform del tilemap es su esquina superior izquierda, no su centro.
    setBounds(map->getOriginX(), map->getOriginY(),
              map->getOriginX() + map->getWorldWidth(),
              map->getOriginY() + map->getWorldHeight());
}

void FollowCamera::applyBounds(float& x, float& y) const {
    if (!hasBounds) return;

    Scene* scene = gameObject->scene;
    if (!scene) return;
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(scene->getRenderer(), &outW, &outH);

    Camera* cam = gameObject->getComponent<Camera>();
    float zoom = (cam && cam->getZoom() > 0.0f) ? cam->getZoom() : 1.0f;

    // La camara marca el CENTRO de la vista: lo que hay que meter dentro de los
    // limites es el rectangulo visible, de ahi las medias pantallas.
    float halfW = (outW * 0.5f) / zoom;
    float halfH = (outH * 0.5f) / zoom;

    float levelW = boundsMaxX - boundsMinX;
    float levelH = boundsMaxY - boundsMinY;

    // Si el nivel cabe entero en pantalla por ese eje, no hay nada que recortar:
    // se centra (si no, los dos topes se pelearian y la camara temblaria).
    if (levelW <= halfW * 2.0f) x = (boundsMinX + boundsMaxX) * 0.5f;
    else {
        if (x < boundsMinX + halfW) x = boundsMinX + halfW;
        if (x > boundsMaxX - halfW) x = boundsMaxX - halfW;
    }

    if (levelH <= halfH * 2.0f) y = (boundsMinY + boundsMaxY) * 0.5f;
    else {
        if (y < boundsMinY + halfH) y = boundsMinY + halfH;
        if (y > boundsMaxY - halfH) y = boundsMaxY - halfH;
    }
}

void FollowCamera::update(float dt) {
    if (!target) return;

    Transform* cam = gameObject->transform;
    Transform* tgt = target->transform;

    // --- Adelanto de la vista (look-ahead) ------------------------------------
    // El adelanto deseado depende de hacia donde se MUEVE el objetivo; se interpola
    // para que al cambiar de direccion la camara no pegue un salto.
    if (lookAhead != 0.0f) {
        float desiredOffset = lookOffset;
        if (hasLastX) {
            float moved = tgt->x - lastTargetX;
            if (moved >  0.001f) desiredOffset =  lookAhead;
            else if (moved < -0.001f) desiredOffset = -lookAhead;
        }
        lastTargetX = tgt->x;
        hasLastX = true;

        float k = lookAheadSpeed * dt;
        if (k > 1.0f) k = 1.0f;
        lookOffset += (desiredOffset - lookOffset) * k;
    } else {
        lookOffset = 0.0f;
    }

    float focusX = tgt->x + lookOffset;

    float halfW = deadZoneWidth  * 0.5f;
    float halfH = deadZoneHeight * 0.5f;

    // Por defecto la camara se queda donde esta...
    float desiredX = cam->x;
    float desiredY = cam->y;

    // ...y solo se reubica si el objetivo cruza el borde de la zona muerta,
    // lo justo para dejarlo de nuevo sobre ese borde.
    float dx = focusX - cam->x;
    if      (dx >  halfW) desiredX = focusX - halfW;
    else if (dx < -halfW) desiredX = focusX + halfW;

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

    // El recorte va AL FINAL: da igual como se haya calculado el destino, la vista
    // nunca se sale del nivel.
    applyBounds(cam->x, cam->y);
}
