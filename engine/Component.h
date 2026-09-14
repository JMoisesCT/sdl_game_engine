#pragma once

class GameObject; // declaracion adelantada

// Clase base de todo componente. NO incluye SDL: es logica pura.

class Component {
public:
    GameObject* gameObject = nullptr;

    virtual ~Component() = default;

    // Al AGREGAR el componente (dentro de addComponent). En este momento el objeto
    // puede no tener todavia sus otros componentes: aqui va la inicializacion propia
    // y el registro en la escena (como hace BoxCollider), no la busqueda de hermanos.
    virtual void awake() {}

    // UNA sola vez, al comienzo del primer frame despues de haberse creado. El objeto
    // ya esta completo: este es el lugar para getComponent<>() de componentes hermanos
    // o para buscar otros objetos de la escena.
    virtual void start() {}

    virtual void update(float dt) {}
    virtual void render() {}

    // Lo llama la fase de fisica cuando este objeto solapa con 'other'.
    // Un componente (Health, etc.) puede sobrescribirlo para reaccionar.
    virtual void onCollision(GameObject* other) {}

private:
    bool started = false;  // ya recibio start()? lo maneja el GameObject
    friend class GameObject;
};
