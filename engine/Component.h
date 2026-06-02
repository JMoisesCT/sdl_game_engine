#pragma once

// Clase base de todo componente. NO incluye SDL: es lógica pura.
// El GameObject dueño se asigna automáticamente al agregar el componente.

class GameObject; // declaración adelantada, evita incluir GameObject.h aquí

class Component {
public:
    GameObject* gameObject = nullptr; // a quién pertenece este componente

    virtual ~Component() = default;

    // Ciclo de vida, igual que en Unity:
    virtual void awake() {}        // al agregarse (ya tiene gameObject asignado)
    virtual void start() {}        // antes del primer frame
    virtual void update(float dt) {} // cada frame, dt en segundos
    virtual void render() {}       // cada frame, despues del update
};
