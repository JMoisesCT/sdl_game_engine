#include "Hazard.h"

#include "GameObject.h"
#include "Transform.h"
#include "Health.h"
#include "RigidBody2D.h"

void Hazard::onCollision(GameObject* other) {
    if (used && !repeat) return;
    if (!other || !other->compareTag(targetTag)) return;

    Health* hp = other->getComponent<Health>();
    if (!hp || !hp->isAlive()) return;

    // Si esta en periodo de gracia, takeDamage no hara nada; comprobarlo ANTES evita
    // gastar el empujon (y el 'used') en un golpe que no cuenta.
    if (hp->isInvulnerable()) return;

    hp->takeDamage(damage);
    used = true;

    // Empujon hacia el lado contrario al peligro. Si el objetivo esta justo encima
    // (misma x), lo mandamos hacia la derecha para no dejarlo caer en vertical sobre
    // el mismo peligro.
    if (RigidBody2D* rb = other->getComponent<RigidBody2D>()) {
        float dx = other->transform->x - gameObject->transform->x;
        float dir = (dx >= 0.0f) ? 1.0f : -1.0f;
        rb->velocityX = dir * knockbackX;
        rb->velocityY = -knockbackY; // en pantalla, -y es hacia arriba
    }
}
