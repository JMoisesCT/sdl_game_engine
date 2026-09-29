#pragma once
#include "../engine/ObjectCatalog.h"

class Scene;
struct LevelData;

// Archivo de nivel del platformer (objetos + camara; a su vez dice que mapa de Tiled
// usar). Lo expone para que main pueda abrirlo en el editor de niveles.
extern const char* PLATFORMER_LEVEL_FILE;

// Lee PLATFORMER_LEVEL_FILE y construye el nivel.
void buildPlatformer(Scene& scene);

// Construye el nivel a partir de datos YA cargados. Es la fabrica que usa el editor:
// reconstruye la escena desde su copia del nivel (lo editado) sin pasar por el disco.
void buildPlatformerLevel(Scene& scene, const LevelData& level);

// Los type que entiende buildPlatformerLevel, con sus propiedades: el editor los ofrece
// en "Nuevo" y avisa de los que no reconoce.
ObjectCatalog platformerObjectCatalog();
