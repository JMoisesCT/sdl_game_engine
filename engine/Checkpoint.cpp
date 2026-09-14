#include "Checkpoint.h"

#include "GameObject.h"
#include "Transform.h"
#include "SpriteAnimator.h"
#include "Respawn.h"

void Checkpoint::start() {
    anim = gameObject->getComponent<SpriteAnimator>(); // opcional
}

void Checkpoint::onCollision(GameObject* other) {
    if (activated) return;
    if (!other || !other->compareTag(targetTag)) return;

    Respawn* resp = other->getComponent<Respawn>();
    if (!resp) return; // quien lo toca no sabe reaparecer: no hay nada que fijar

    activated = true;
    resp->setPoint(gameObject->transform->x + offsetX,
                   gameObject->transform->y + offsetY);

    if (anim && !activateAnimation.empty()) {
        // La animacion de activacion no hace loop; al terminar se pasa a la de reposo
        // (bandera ondeando) si se indico una.
        if (!idleAnimation.empty()) {
            SpriteAnimator* a = anim;
            std::string idle = idleAnimation;
            anim->setOnComplete([a, idle](const std::string&) { a->play(idle); });
        }
        anim->play(activateAnimation);
    }

    if (onActivate) onActivate();
}
