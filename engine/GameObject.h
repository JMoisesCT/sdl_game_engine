#pragma once
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <string>
#include <utility>
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

    template <typename T, typename... Args>
    T* addComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value,
                      "T debe heredar de Component");
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->gameObject = this;
        T* ptr = comp.get();
        components[std::type_index(typeid(T))] = std::move(comp);
        ptr->awake();
        return ptr;
    }

    template <typename T>
    T* getComponent() {
        auto it = components.find(std::type_index(typeid(T)));
        if (it == components.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    void start()          { for (auto& [type, c] : components) c->start(); }
    void update(float dt) { for (auto& [type, c] : components) c->update(dt); }
    void render()         { for (auto& [type, c] : components) c->render(); }

    void notifyCollision(GameObject* other) {
        for (auto& [type, c] : components) c->onCollision(other);
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;
};
