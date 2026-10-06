#pragma once
#include <string>
#include <vector>
#include <map>

#include "TiledObjectLayer.h" // TiledObject: los objetos del nivel son los mismos datos planos

class Camera;
class FollowCamera;
class Scene;
class TilemapRenderer;
class ObjectCatalog;

// Un NIVEL son dos archivos, y cada uno tiene UN SOLO programa que lo escribe:
//
//   nivel.json        -> el MAPA. Lo escribe solo Tiled: capas de tiles, tileset y que
//                        tiles son solidos. El motor solo lo lee (TilemapRenderer).
//   nivel.level.json  -> lo que va ENCIMA del mapa: objetos (jugador, items, trampas...)
//                        y ajustes del nivel (camara). Lo escribira solo el editor del
//                        motor; mientras tanto se puede editar a mano.
//
// Por que separados: si los dos programas escribieran el mismo archivo, guardar desde
// uno pisaria lo que se cambio en el otro con la copia vieja que tenia en memoria
// (pintas una plataforma en Tiled, mueves una fruta en el editor, guardas... y la
// plataforma desaparece). Con un escritor por archivo ese conflicto no puede existir.
//
// GENERICO: igual que con la capa de objetos de Tiled, el motor NO sabe que significa
// cada "type" de objeto. Solo entrega los datos; la fabrica que decide que crear vive
// en game/.
struct LevelData {
    // Ruta del mapa de Tiled YA RESUELTA (el archivo la guarda relativa a su carpeta,
    // igual que Tiled guarda la imagen del tileset relativa al mapa).
    std::string mapPath;
    // La misma ruta TAL COMO esta escrita en el archivo (relativa). Es la que se vuelve
    // a escribir al guardar.
    std::string mapFile;

    // Siguiente id libre para un objeto nuevo (lo usara el editor al crear objetos).
    int nextObjectId = 1;

    // Ajustes de camara que trae el archivo, por nombre ("zoom", "deadZoneWidth"...).
    // Solo estan los que el archivo define: ver applyCameraSettings.
    std::map<std::string, double> camera;

    // Objetos del nivel. Mismo struct que la capa de objetos de Tiled, asi la fabrica
    // del juego no cambia: cx,cy es el CENTRO en pixeles del mapa (sin escalar), y las
    // propiedades van separadas en stringProps / numberProps.
    std::vector<TiledObject> objects;
};

// Lee un archivo .level.json. Devuelve false (y hace SDL_Log) si no se puede abrir o no
// es valido; en ese caso 'out' queda sin tocar. Ver el .cpp para el formato.
bool loadLevel(const std::string& path, LevelData& out);

// Escribe el nivel en un .level.json (el mismo formato que lee loadLevel). Primero lo
// escribe en un archivo temporal y despues lo renombra: si algo falla a mitad de camino,
// el archivo anterior queda intacto. Devuelve false (y hace SDL_Log) si no pudo.
// Solo el editor deberia llamarla: el .level.json tiene UN solo escritor.
bool saveLevel(const std::string& path, const LevelData& level);

// Aplica a la camara los ajustes que traiga el nivel, SOLO los que esten en el archivo:
// lo que falte conserva el valor que ya tenia el componente. Asi el juego pone sus
// valores por defecto y el nivel los sobreescribe donde quiera.
//   Camera:       zoom
//   FollowCamera: deadZoneWidth, deadZoneHeight, smoothSpeed, lookAhead, lookAheadSpeed
// Cualquiera de los dos punteros puede ser nullptr.
void applyCameraSettings(const LevelData& level, Camera* cam, FollowCamera* follow);

// Los nombres que entiende applyCameraSettings, en el orden en que se escriben al guardar
// (el editor los muestra en ese mismo orden).
const std::vector<std::string>& levelCameraKeys();

// Crea en la escena los objetos del nivel, cada uno con la funcion setup de su type en
// el catalogo del juego (ver ObjectCatalog.h). Por cada objeto del archivo:
//   1) crea el GameObject (con el name del objeto, o su type si no tiene),
//   2) lo pone en su centro en el MUNDO (pixeles del mapa -> mundo con 'map'),
//   3) le pone levelObjectId (lo que usa el editor para reconocerlo),
//   4) y llama al setup de su type, que le agrega los componentes.
// Un type que no esta en el catalogo se avisa en el log y se salta. 'map' es el tilemap
// del nivel (nullptr = sin mapa: los pixeles del archivo son ya el mundo).
// Los objetos se crean en el orden del archivo.
void spawnLevelObjects(Scene& scene, const LevelData& level, const TilemapRenderer* map,
                       const ObjectCatalog& catalog);
