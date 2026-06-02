#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <string>
#include <utility>
#include <type_traits>

#include "Component.h"
#include "Transform.h"

class Scene; // declaracion adelantada

// Un GameObject NO es nada por herencia: TIENE componentes.
// Guarda un componente por tipo, indexado por su type_index (la llave es el tipo).
// Es el mismo modelo de Unity: addComponent<T>() / getComponent<T>().

class GameObject {
public:
    std::string name;
    Scene* scene = nullptr;            // la escena dueña, la asigna Scene::createGameObject
    Transform* transform = nullptr;    // acceso directo: todo objeto tiene Transform

    explicit GameObject(std::string n = "GameObject") : name(std::move(n)) {
        transform = addComponent<Transform>(); // todo objeto nace con un Transform
    }

    // Construye un componente T con sus argumentos, lo asocia a este objeto y lo guarda.
    // Devuelve un puntero NO dueno para usarlo (el GameObject sigue siendo el dueno).
    template <typename T, typename... Args>
    T* addComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value,
                      "T debe heredar de Component");

        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->gameObject = this;
        T* ptr = comp.get();
        components[std::type_index(typeid(T))] = std::move(comp);
        ptr->awake(); // ya tiene gameObject y scene asignados
        return ptr;
    }

    // Devuelve el componente del tipo pedido, o nullptr si no existe.
    template <typename T>
    T* getComponent() {
        auto it = components.find(std::type_index(typeid(T)));
        if (it == components.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    void start()           { for (auto& [type, c] : components) c->start(); }
    void update(float dt)  { for (auto& [type, c] : components) c->update(dt); }
    void render()          { for (auto& [type, c] : components) c->render(); }

private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;
};
