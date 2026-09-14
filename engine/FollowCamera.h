#pragma once
#include "Component.h"

class GameObject;
class TilemapRenderer;

// Mueve la camara para seguir a un objetivo, pero SOLO cuando este sale de la
// "zona muerta": un rectangulo centrado en la camara. Mientras el objetivo se
// mueve dentro de la zona, la camara no se mueve (evita temblores y mareos).

class FollowCamera : public Component {
public:
    float deadZoneWidth  = 100.0f; // ancho de la zona muerta (unidades de mundo)
    float deadZoneHeight = 100.0f; // alto de la zona muerta
    float smoothSpeed    = 0.0f;   // 0 = seguimiento instantaneo; >0 = suavizado

    // Adelanta la vista en la direccion en que mira/anda el objetivo: en vez de
    // centrarlo, deja mas pantalla del lado al que va, para ver lo que viene.
    // Son pixeles de mundo; 0 lo desactiva.
    float lookAhead      = 0.0f;
    float lookAheadSpeed = 4.0f;   // que tan rapido se desplaza ese adelanto

    void setTarget(GameObject* t) { target = t; }

    // --- Limites del encuadre ----------------------------------------------------
    // Sin limites la camara sigue al jugador hasta fuera del nivel y se ve el vacio
    // de los bordes. Con limites, el RECTANGULO VISIBLE se mantiene dentro de la zona
    // indicada (en coordenadas de mundo). Si el nivel es mas pequeno que la pantalla
    // en un eje, en ese eje la camara se queda centrada en el nivel.
    void setBounds(float minX, float minY, float maxX, float maxY);
    void clearBounds() { hasBounds = false; }

    // Atajo: toma los limites del tamano del tilemap (origen + ancho/alto de mundo).
    // Es lo normal en un plataformas: la camara no se sale del mapa.
    void setBoundsFromTilemap(const TilemapRenderer* map);

    void update(float dt) override;

private:
    // Deja (x, y) dentro de los limites, teniendo en cuenta media pantalla y el zoom.
    void applyBounds(float& x, float& y) const;

    GameObject* target = nullptr; // a quien seguir (NO somos dueno)

    bool  hasBounds = false;
    float boundsMinX = 0.0f, boundsMinY = 0.0f, boundsMaxX = 0.0f, boundsMaxY = 0.0f;

    float lookOffset = 0.0f;   // adelanto actual, interpolado hacia el deseado
    float lastTargetX = 0.0f;  // para saber hacia donde se mueve el objetivo
    bool  hasLastX = false;
};
