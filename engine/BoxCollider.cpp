#include "BoxCollider.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"

void BoxCollider::awake() {
    gameObject->scene->registerCollider(this);
}

float BoxCollider::centerX() const { return gameObject->transform->x + offsetX; }
float BoxCollider::centerY() const { return gameObject->transform->y + offsetY; }
