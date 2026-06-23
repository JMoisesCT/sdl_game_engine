#pragma once
#include <vector>
#include <memory>
#include <string>

#include "GameObject.h"
#include "AssetManager.h"

struct SDL_Renderer;
class Camera;
class BoxCollider;

class Scene {
public:
    explicit Scene(SDL_Renderer* renderer)
        : renderer(renderer), assets(renderer) {}

    GameObject* createGameObject(const std::string& name = "GameObject") {
        auto obj = std::make_unique<GameObject>(name);
        obj->scene = this;
        GameObject* ptr = obj.get();
        objects.push_back(std::move(obj));
        ptr->start();
        return ptr;
    }

    // Marca un objeto para destruirse. No lo borra en el acto: se elimina al
    // final del frame (asi es seguro llamarlo desde un update o una colision).
    void destroy(GameObject* obj) { if (obj) obj->alive = false; }

    void update(float dt) {
        // Conteo fijo: los objetos que se creen durante el frame (spawners) se
        // agregan al final y NO se actualizan hasta el siguiente frame.
        size_t count = objects.size();
        for (size_t i = 0; i < count; ++i) objects[i]->update(dt);

        resolveCollisions();
        removeDeadObjects();
    }

    void render() { for (auto& o : objects) o->render(); }

    SDL_Renderer* getRenderer() const  { return renderer; }
    AssetManager& getAssets()          { return assets; }

    Camera* getActiveCamera() const    { return activeCamera; }
    void    setActiveCamera(Camera* c) { activeCamera = c; }

    void registerCollider(BoxCollider* c) { colliders.push_back(c); }
    const std::vector<BoxCollider*>& getColliders() const { return colliders; }

private:
    void resolveCollisions();  // Scene.cpp
    void removeDeadObjects();  // Scene.cpp

    SDL_Renderer* renderer = nullptr;
    AssetManager  assets;
    Camera*       activeCamera = nullptr;
    std::vector<std::unique_ptr<GameObject>> objects;
    std::vector<BoxCollider*> colliders;
};
