#pragma once
#include "Component.h"

// Rectangulo de colision (AABB) anclado al centro del Transform.
// El tamano va en unidades de mundo (no se escala solo con el Transform).

class BoxCollider : public Component {
public:
    float width  = 0.0f;
    float height = 0.0f;
    float offsetX = 0.0f;     // corrimiento respecto al centro del objeto
    float offsetY = 0.0f;
    bool  isTrigger = false;  // true = solo avisa (onCollision), no bloquea

    void awake() override;     // se registra en la escena

    // AABB en coordenadas de mundo (lo consulta la fase de fisica).
    float centerX() const;
    float centerY() const;
    float halfW() const { return width  * 0.5f; }
    float halfH() const { return height * 0.5f; }
};
