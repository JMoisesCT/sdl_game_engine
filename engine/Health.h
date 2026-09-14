#pragma once
#include <functional>
#include "Component.h"

class SpriteRenderer;

// Vida de un objeto: puntos, dano, muerte e invulnerabilidad temporal con parpadeo.
// Es generico: no sabe quien le pega ni que pasa al morir. Lo que ocurre lo decide el
// juego con dos callbacks:
//
//     auto hp = player->addComponent<Health>();
//     hp->maxHP = 3;
//     hp->onDamage = [](int d) { /* sonido, sacudir la camara... */ };
//     hp->onDeath  = [resp]    { resp->die(); };   // p.ej. reaparecer
//
// INVULNERABILIDAD: tras recibir dano, el objeto no vuelve a recibirlo durante
// 'invulnerabilityTime'. Sin eso, quedarse encima de unos pinchos vacia la vida en un
// puñado de frames, porque la colision se avisa CADA frame que dure el solape.
// Mientras dura, si el objeto tiene SpriteRenderer, parpadea para que se note.

class Health : public Component {
public:
    int   maxHP = 3;
    float invulnerabilityTime = 1.0f;  // segundos de gracia tras un golpe (0 = sin gracia)
    float blinkInterval = 0.08f;       // cada cuanto se apaga/enciende el sprite

    std::function<void(int amount)> onDamage; // recibe cuanto dano se aplico
    std::function<void()> onDeath;            // al llegar a 0 (una sola vez)

    int  getHP() const { return hp; }
    bool isAlive() const { return hp > 0; }
    bool isInvulnerable() const { return invulnerable > 0.0f; }

    // Quita vida. No hace nada si ya esta muerto o invulnerable. Si la vida llega a 0
    // llama a onDeath una unica vez.
    void takeDamage(int amount);

    // Mata en el acto, IGNORANDO la invulnerabilidad (caer al vacio, un aplastamiento).
    void kill();

    void heal(int amount);

    // Devuelve la vida al maximo y quita la invulnerabilidad. Lo usa el respawn.
    void resetHealth();

    void awake() override;
    void start() override;
    void update(float dt) override;

private:
    void setSpriteVisible(bool v);

    int   hp = 0;
    float invulnerable = 0.0f; // tiempo que queda de gracia
    float blinkTimer = 0.0f;
    SpriteRenderer* sprite = nullptr; // opcional; solo para el parpadeo
};
