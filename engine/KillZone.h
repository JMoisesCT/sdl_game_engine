#pragma once
#include <string>
#include "Component.h"

class GameObject;

// Zona que mata en el acto a lo que entre en ella. Lo tipico: una franja ancha por
// debajo del nivel, para que caerse del mapa no deje al jugador cayendo para siempre.
// Tambien vale para lava, un techo que aplasta o el fondo de un pozo.
//
// Necesita un BoxCollider con isTrigger = true, normalmente muy grande.
//
// A diferencia de Hazard, IGNORA la invulnerabilidad: caer al vacio mata aunque
// acabes de recibir un golpe. Por eso llama a Health::kill() y no a takeDamage().

class KillZone : public Component {
public:
    std::string targetTag = "Player";

    void onCollision(GameObject* other) override;
};
