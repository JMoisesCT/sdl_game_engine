#pragma once
#include "Component.h"

// Define que parte del mundo se ve. La posicion la da el Transform del objeto:
// ese punto del mundo queda en el CENTRO de la pantalla.
// Mover el Transform de la camara = desplazar la vista (seguir al jugador, etc).

class Camera : public Component {
public:
    float zoom = 1.0f; // 1 = normal, 2 = acercado, 0.5 = alejado

    void awake() override; // se registra como la camara activa de la escena

    // Convierte un punto del mundo a su posicion en pantalla (aplica posicion + zoom).
    void worldToScreen(float worldX, float worldY, float& screenX, float& screenY) const;

    // La inversa: un punto de la pantalla (p. ej. el raton) -> punto del mundo que se ve
    // ahi. Es lo que permite hacer clic sobre un objeto del mundo.
    void screenToWorld(float screenX, float screenY, float& worldX, float& worldY) const;

    float getZoom() const { return zoom; }
};
