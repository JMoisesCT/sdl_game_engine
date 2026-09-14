#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

#include "Component.h"
#include "Transform.h"

class Scene;

class GameObject {
public:
    std::string name;
    Scene* scene = nullptr;
    Transform* transform = nullptr;
    bool alive = true; // false = marcado para destruir; la Scene lo barre al final del frame

    explicit GameObject(std::string n = "GameObject") : name(std::move(n)) {
        transform = addComponent<Transform>();
    }

    // Agrega un componente y devuelve un puntero CRUDO no-dueno (el dueno es este
    // GameObject). El orden en que se agregan ES el orden en que se actualizan y se
    // dibujan: agregar el controlador ANTES del RigidBody2D hace que el input escriba
    // la velocidad en el mismo frame en que la fisica la integra (sin latencia).
    template <typename T, typename... Args>
    T* addComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value,
                      "T debe heredar de Component");
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->gameObject = this;
        T* ptr = comp.get();

        std::type_index type(typeid(T));
        auto it = index.find(type);
        if (it == index.end()) {
            components.push_back(std::move(comp));
        } else {
            // Ya habia uno de ese tipo: lo reemplazamos EN SU SITIO, para no cambiarle
            // el turno de actualizacion al resto de los componentes.
            for (auto& slot : components) {
                if (slot.get() == it->second) { slot = std::move(comp); break; }
            }
        }
        index[type] = ptr;

        ptr->awake();
        return ptr;
    }

    template <typename T>
    T* getComponent() {
        auto it = index.find(std::type_index(typeid(T)));
        if (it == index.end()) return nullptr;
        return static_cast<T*>(it->second);
    }

    // Llama start() UNA sola vez a cada componente que todavia no lo recibio. Lo hace
    // la Scene al comienzo del frame, cuando el objeto ya tiene TODOS sus componentes
    // (por eso start es el lugar para buscar componentes hermanos, y awake no).
    void startPending() {
        // La condicion se reevalua: si un start() agrega componentes, tambien arrancan.
        for (size_t i = 0; i < components.size(); ++i) {
            Component* c = components[i].get();
            if (!c->started) { c->started = true; c->start(); }
        }
    }

    void update(float dt) {
        // Conteo fijo: un componente agregado durante el update no se actualiza hasta
        // el frame siguiente (primero necesita su start).
        size_t count = components.size();
        for (size_t i = 0; i < count; ++i) components[i]->update(dt);
    }

    void render() {
        size_t count = components.size();
        for (size_t i = 0; i < count; ++i) components[i]->render();
    }

    void notifyCollision(GameObject* other) {
        size_t count = components.size();
        for (size_t i = 0; i < count; ++i) components[i]->onCollision(other);
    }

private:
    // Dueno de los componentes, EN ORDEN DE INSERCION: ese es el orden de update y
    // render, y es determinista (antes vivian en el unordered_map y el orden dependia
    // del hash del tipo: el input podia correr despues de la fisica).
    std::vector<std::unique_ptr<Component>> components;
    // Indice por tipo para que getComponent<T>() siga siendo O(1). No es dueno.
    std::unordered_map<std::type_index, Component*> index;
};
