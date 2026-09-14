#include "Health.h"

#include "GameObject.h"
#include "SpriteRenderer.h"

void Health::awake() {
    hp = maxHP; // el maxHP puede cambiarse despues; resetHealth() lo vuelve a aplicar
}

void Health::start() {
    sprite = gameObject->getComponent<SpriteRenderer>(); // puede no haber: es opcional
}

void Health::takeDamage(int amount) {
    if (!isAlive() || amount <= 0) return;
    if (invulnerable > 0.0f) return; // en periodo de gracia: el golpe no cuenta

    hp -= amount;
    if (hp < 0) hp = 0;

    if (onDamage) onDamage(amount);

    if (hp == 0) {
        setSpriteVisible(true);   // que no se quede apagado a media parpadeo
        invulnerable = 0.0f;
        if (onDeath) onDeath();
        return;
    }

    invulnerable = invulnerabilityTime;
    blinkTimer = 0.0f;
}

void Health::kill() {
    if (!isAlive()) return;
    hp = 0;
    setSpriteVisible(true);
    invulnerable = 0.0f;
    if (onDeath) onDeath();
}

void Health::heal(int amount) {
    if (amount <= 0) return;
    hp += amount;
    if (hp > maxHP) hp = maxHP;
}

void Health::resetHealth() {
    hp = maxHP;
    invulnerable = 0.0f;
    blinkTimer = 0.0f;
    setSpriteVisible(true);
}

void Health::update(float dt) {
    if (invulnerable <= 0.0f) return;

    invulnerable -= dt;
    if (invulnerable <= 0.0f) {
        invulnerable = 0.0f;
        setSpriteVisible(true); // al acabar la gracia, siempre visible
        return;
    }

    // Parpadeo: encender/apagar el sprite a intervalos fijos mientras dure la gracia.
    blinkTimer += dt;
    if (blinkTimer >= blinkInterval) {
        blinkTimer -= blinkInterval;
        if (sprite) sprite->visible = !sprite->visible;
    }
}

void Health::setSpriteVisible(bool v) {
    if (sprite) sprite->visible = v;
}
