#pragma once
#include <string>
#include "Component.h"

class GameObject;

// Hace dano a lo que lo toque, si ese algo lleva el tag indicado y tiene Health.
// Sirve para pinchos, sierras, fuego, enemigos que danan al contacto... El objeto
// necesita un BoxCollider (normalmente isTrigger = true, para que el jugador no se
// quede frenado contra los pinchos).
//
//     auto h = pinchos->addComponent<Hazard>();
//     h->damage = 1;
//     h->targetTag = "Player";
//
// EMPUJON: al golpear, lanza al objetivo hacia el lado contrario al del peligro. No es
// un adorno: sin el, el jugador se queda encima del peligro y vuelve a cobrar en cuanto
// se le acaba la invulnerabilidad. Necesita que el objetivo tenga RigidBody2D.

class Hazard : public Component {
public:
    int         damage = 1;
    std::string targetTag = "Player"; // a quien hace dano (vacio = a nadie)

    float knockbackX  = 300.0f; // impulso horizontal, alejandose del peligro
    float knockbackY  = 360.0f; // impulso hacia ARRIBA (en pantalla, -y)

    // Si es false, solo hace dano una vez (pinchos de un solo uso, proyectiles).
    bool repeat = true;

    void onCollision(GameObject* other) override;

private:
    bool used = false;
};
