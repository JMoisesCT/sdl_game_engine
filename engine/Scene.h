#pragma once
#include <vector>
#include <memory>
#include <string>

#include "GameObject.h"
#include "AssetManager.h"

struct SDL_Renderer; // declaracion adelantada: no incluimos SDL aqui

// La escena contiene los objetos, guarda (sin ser dueno) el renderer del alumno
// y POSEE el AssetManager que comparte las texturas entre todos los objetos.

class Scene {
public:
    explicit Scene(SDL_Renderer* renderer)
        : renderer(renderer), assets(renderer) {}

    // Crea un objeto dentro de la escena. Usar SIEMPRE este metodo.
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

    SDL_Renderer* getRenderer() const { return renderer; }
    AssetManager& getAssets()         { return assets; }

private:
    SDL_Renderer* renderer = nullptr; // del alumno, NO somos dueno
    AssetManager  assets;             // construido con el renderer de arriba
    std::vector<std::unique_ptr<GameObject>> objects;
};
