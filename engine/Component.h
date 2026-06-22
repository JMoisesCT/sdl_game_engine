#pragma once

class GameObject; // declaracion adelantada

// Clase base de todo componente. NO incluye SDL: es logica pura.

class Component {
public:
    GameObject* gameObject = nullptr;

    virtual ~Component() = default;

    virtual void awake() {}
    virtual void start() {}
    virtual void update(float dt) {}
    virtual void render() {}

    // Lo llama la fase de fisica cuando este objeto solapa con 'other'.
    // Un componente (Health, etc.) puede sobrescribirlo para reaccionar.
    virtual void onCollision(GameObject* other) {}
};
