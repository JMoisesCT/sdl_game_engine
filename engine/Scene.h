#pragma once
#include <vector>
#include <memory>
#include <string>

#include "GameObject.h"
#include "AssetManager.h"

struct SDL_Renderer; // declaracion adelantada
class Camera;
class BoxCollider;

// Contiene los objetos, posee el AssetManager, conoce la camara activa y
// ejecuta la fase de fisica (deteccion + resolucion de colisiones).

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

    void update(float dt) {
        for (auto& o : objects) o->update(dt); // mover (RigidBody, input, etc.)
        resolveCollisions();                    // luego corregir choques
    }

    void render() { for (auto& o : objects) o->render(); }

    SDL_Renderer* getRenderer() const  { return renderer; }
    AssetManager& getAssets()          { return assets; }

    Camera* getActiveCamera() const    { return activeCamera; }
    void    setActiveCamera(Camera* c) { activeCamera = c; }

    void registerCollider(BoxCollider* c) { colliders.push_back(c); }

private:
    void resolveCollisions(); // definida en Scene.cpp

    SDL_Renderer* renderer = nullptr;
    AssetManager  assets;
    Camera*       activeCamera = nullptr;
    std::vector<std::unique_ptr<GameObject>> objects;
    std::vector<BoxCollider*> colliders; // no somos dueno; viven en sus objetos
};
