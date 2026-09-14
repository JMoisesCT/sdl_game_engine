#pragma once
#include <vector>
#include <memory>
#include <string>

#include "GameObject.h"
#include "AssetManager.h"

struct SDL_Renderer;
class Camera;
class BoxCollider;
class TilemapCollider;

class Scene {
public:
    explicit Scene(SDL_Renderer* renderer)
        : renderer(renderer), assets(renderer) {}

    GameObject* createGameObject(const std::string& name = "GameObject") {
        auto obj = std::make_unique<GameObject>(name);
        obj->scene = this;
        GameObject* ptr = obj.get();
        objects.push_back(std::move(obj));
        // OJO: aqui NO se llama start(). El objeto recien creado todavia no tiene
        // ningun componente aparte de su Transform: los agrega el codigo que viene
        // despues de este return. El start() de cada componente lo dispara
        // Scene::update al comienzo del frame siguiente, con el objeto ya completo.
        return ptr;
    }

    // Marca un objeto para destruirse. No lo borra en el acto: se elimina al
    // final del frame (asi es seguro llamarlo desde un update o una colision).
    void destroy(GameObject* obj) { if (obj) obj->alive = false; }

    void update(float dt) {
        // Conteo fijo: los objetos que se creen durante el frame (spawners) se
        // agregan al final y NO se actualizan hasta el siguiente frame.
        size_t count = objects.size();
        // 1) start() pendiente: en su propia pasada y ANTES de los updates, para que un
        //    start pueda mirar a otros objetos que ya arrancaron en este mismo frame.
        for (size_t i = 0; i < count; ++i) objects[i]->startPending();
        // 2) update de todos.
        for (size_t i = 0; i < count; ++i) objects[i]->update(dt);

        resolveCollisions();
        removeDeadObjects();
    }

    void render();  // Scene.cpp: dibuja por capas (GameObject::sortingOrder)

    SDL_Renderer* getRenderer() const  { return renderer; }
    AssetManager& getAssets()          { return assets; }

    Camera* getActiveCamera() const    { return activeCamera; }
    void    setActiveCamera(Camera* c) { activeCamera = c; }

    void registerCollider(BoxCollider* c) { colliders.push_back(c); }
    const std::vector<BoxCollider*>& getColliders() const { return colliders; }

    // Los tilemaps solidos se registran aparte: no son un collider mas en el bucle de
    // pares, son geometria del nivel a la que la fisica le PREGUNTA por celdas.
    void registerTilemapCollider(TilemapCollider* t) { tilemaps.push_back(t); }

private:
    void resolveCollisions();         // Scene.cpp: orquesta las dos fases
    void resolveTilemapCollisions();  // cuerpos contra la geometria del nivel
    void resolvePairCollisions();     // collider contra collider (AABB, O(n^2))
    void removeDeadObjects();         // Scene.cpp

    SDL_Renderer* renderer = nullptr;
    AssetManager  assets;
    Camera*       activeCamera = nullptr;
    std::vector<std::unique_ptr<GameObject>> objects;
    std::vector<BoxCollider*> colliders;
    std::vector<TilemapCollider*> tilemaps;
    std::vector<GameObject*> drawList; // reutilizado cada frame para ordenar el dibujo
};
