#pragma once
#include <functional>
#include "GameObject.h"
#include "Scene.h"

// Llama a la funcion 'spawn' cada 'interval' segundos. La funcion recibe la
// escena y crea/configura lo que quieras (enemigos, obstaculos, items).

class Spawner : public Component {
public:
    float interval = 1.0f;
    std::function<void(Scene&)> spawn; // el alumno la define con una lambda

    void update(float dt) override {
        if (!spawn) return;
        timer += dt;
        while (timer >= interval) {
            timer -= interval;
            spawn(*gameObject->scene);
        }
    }

private:
    float timer = 0.0f;
};
