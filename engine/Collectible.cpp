#include "Collectible.h"

#include "GameObject.h"
#include "Scene.h"
#include "SpriteAnimator.h"

void Collectible::start() {
    anim = gameObject->getComponent<SpriteAnimator>(); // opcional
}

void Collectible::onCollision(GameObject* other) {
    if (taken) return;
    if (!other || !other->compareTag(collectorTag)) return;

    taken = true;
    if (onCollect) onCollect(other);

    if (!destroyOnCollect) return;

    // Con animacion de recogida: se destruye al terminar el clip, no antes, para que
    // el efecto se vea entero. Sin ella, se destruye ya.
    if (anim && !collectAnimation.empty()) {
        GameObject* self = gameObject;
        anim->setOnComplete([self](const std::string&) { self->scene->destroy(self); });
        anim->play(collectAnimation);
    } else {
        gameObject->scene->destroy(gameObject);
    }
}
