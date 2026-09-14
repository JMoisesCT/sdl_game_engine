#pragma once
#include "Component.h"

class TilemapRenderer;
class BoxCollider;
class RigidBody2D;

// Hace SOLIDAS las celdas que el TilemapRenderer del MISMO objeto marco con setSolid
// (o con la propiedad "solid" del tileset de Tiled). Se agrega junto al renderer:
//
//     auto tm = suelo->addComponent<TilemapRenderer>();
//     tm->loadFromTiledJson("assets/maps/nivel.json");
//     suelo->addComponent<TilemapCollider>();   // sin esto, el mapa no frena a nadie
//
// NO crea un collider por celda: se registra en la escena y la fase de fisica le
// pregunta al mapa por las celdas que toca cada cuerpo. Con un nivel de verdad eso son
// cientos de colliders menos en un bucle O(n^2).
//
// RESOLUCION POR EJE: el cuerpo se separa primero en X (usando la Y que tenia el frame
// anterior) y despues en Y. Resolver los dos ejes a la vez, por el lado de menor
// penetracion, es lo que hace que un personaje se enganche en las COSTURAS entre dos
// tiles vecinos del suelo: al correr, la penetracion vertical minima se confunde con un
// choque lateral contra el tile siguiente y lo frena en seco. Separando por eje, la
// pasada X solo ve paredes de verdad y la pasada Y solo ve suelos y techos.
//
// APOYO ESTABLE: ademas de la separacion, se sondea una franja fina bajo los pies para
// marcar 'grounded'. Sin esa sonda el apoyo dependeria de que el cuerpo penetre el suelo
// en ese frame, que a framerate alto es una fraccion de pixel (o nada): el 'grounded'
// parpadearia y el salto fallaria a veces.

class TilemapCollider : public Component {
public:
    // Grosor (en pixeles de mundo) de la franja que se mira bajo los pies para decidir
    // si el cuerpo esta apoyado. Un par de pixeles basta y no se nota.
    float groundProbe = 2.0f;

    void awake() override; // se registra en la escena
    void start() override; // resuelve el TilemapRenderer hermano

    TilemapRenderer* getMap() const { return map; }

    // Separa un cuerpo de las celdas solidas y actualiza su 'grounded'. La llama la
    // fase de fisica de la Scene; no hace falta invocarla a mano.
    void resolveBody(BoxCollider* box, RigidBody2D* rb);

private:
    // Cuanto hay que mover el cuerpo en un eje para sacarlo de las celdas solidas.
    // 'dir' es el sentido en que se movio en ese eje (>0, <0 o 0 si no se movio).
    float pushOutX(BoxCollider* box, float dir) const;
    float pushOutY(BoxCollider* box, float dir) const;

    TilemapRenderer* map = nullptr; // hermano; no somos dueno
};
