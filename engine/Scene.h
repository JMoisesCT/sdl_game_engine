#pragma once
#include <vector>
#include <memory>
#include <string>

#include "GameObject.h"
#include "AssetManager.h"

struct SDL_Renderer; // declaracion adelantada: no incluimos SDL aqui
class Camera;        // declaracion adelantada: solo guardamos un puntero

// La escena contiene los objetos, guarda el renderer del alumno (sin ser dueno),
// POSEE el AssetManager y conoce cual es la camara activa.

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

    void update(float dt) { for (auto& o : objects) o->update(dt); }
    void render()         { for (auto& o : objects) o->render(); }

    SDL_Renderer* getRenderer() const  { return renderer; }
    AssetManager& getAssets()          { return assets; }

    // Camara activa (es un componente de algun GameObject; NO somos dueno).
    Camera* getActiveCamera() const    { return activeCamera; }
    void    setActiveCamera(Camera* c) { activeCamera = c; }

private:
    SDL_Renderer* renderer = nullptr;
    AssetManager  assets;
    Camera*       activeCamera = nullptr;
    std::vector<std::unique_ptr<GameObject>> objects;
};
