#include "KillZone.h"

#include "GameObject.h"
#include "Health.h"

void KillZone::onCollision(GameObject* other) {
    if (!other || !other->compareTag(targetTag)) return;

    if (Health* hp = other->getComponent<Health>())
        hp->kill(); // kill y no takeDamage: esto mata pase lo que pase
}
