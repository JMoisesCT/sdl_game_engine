#pragma once
#include "Component.h"

class GameObject;

// Mueve la camara para seguir a un objetivo, pero SOLO cuando este sale de la
// "zona muerta": un rectangulo centrado en la camara. Mientras el objetivo se
// mueve dentro de la zona, la camara no se mueve (evita temblores y mareos).

class FollowCamera : public Component {
public:
    float deadZoneWidth  = 100.0f; // ancho de la zona muerta (unidades de mundo)
    float deadZoneHeight = 100.0f; // alto de la zona muerta
    float smoothSpeed    = 0.0f;   // 0 = seguimiento instantaneo; >0 = suavizado

    void setTarget(GameObject* t) { target = t; }

    void update(float dt) override;

private:
    GameObject* target = nullptr; // a quien seguir (NO somos dueno)
};
