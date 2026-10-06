#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "TiledObjectLayer.h" // TiledObject: de ahi salen las propiedades de cada objeto

class GameObject;

// CATALOGO DE OBJETOS: la lista de los objetos de UN juego. Cada entrada es un "type"
// (el nombre que se guarda en el .level.json) con la funcion que ARMA ese objeto: la que
// le agrega sus componentes. Lo escribe el JUEGO y lo usan dos:
//   - la fabrica del nivel, que con spawnLevelObjects (LevelData.h) crea cada objeto
//     del archivo llamando a la funcion de su type;
//   - el editor de niveles, que ofrece esos type en "Nuevo", muestra la ayuda, pone
//     las propiedades por defecto y avisa de los type o valores que no existen.
// Como los dos leen la MISMA lista, no pueden decir cosas distintas.
//
//     // La receta de un objeto. El motor ya lo creo y lo puso en su sitio (y con
//     // el id que usa el editor): aqui solo se le agregan componentes.
//     static void setupSpikes(GameObject* obj, const LevelObject& o) {
//         obj->tag = "Hazard";
//         obj->addComponent<SpriteRenderer>("assets/.../Spikes.png");
//         obj->addComponent<BoxCollider>()->isTrigger = true;
//         obj->addComponent<Hazard>()->targetTag = "Player";
//     }
//
//     ObjectCatalog misObjetos() {
//         ObjectCatalog c;
//         c.add("PlayerStart", setupPlayer, "Donde aparece el jugador").playerSpawn();
//         c.add("Fruit", setupFruit, "Fruta que se recoge")
//             .choice("fruit", { "Apple", "Bananas", "Cherries" });
//         c.add("Spikes", setupSpikes, "Pinchos: quitan vida al tocarlos");
//         return c;
//     }
//
// Agregar un objeto nuevo al juego = escribir su setup y una linea c.add(...). Despues
// se coloca con el editor (F2 -> Nuevo).

// Lo que recibe la funcion setup de un type: el objeto del nivel ya convertido al MUNDO
// (el GameObject ya esta en x,y) y sus propiedades.
struct LevelObject {
    int   id = 0;
    std::string type;
    std::string name;
    float x = 0.0f, y = 0.0f; // centro en el MUNDO (ya aplicado al Transform)
    float w = 0.0f, h = 0.0f; // tamanio en el MUNDO (0 = objeto punto; >0 = una zona)

    // Propiedades del archivo, con valor por defecto si faltan.
    std::string getString(const std::string& key, const std::string& def = "") const {
        return data ? data->getString(key, def) : def;
    }
    double getNumber(const std::string& key, double def = 0.0) const {
        return data ? data->getNumber(key, def) : def;
    }

    const TiledObject* data = nullptr; // la entrada del archivo tal cual (pixeles del mapa)
};

// Una propiedad que el juego espera en un type (la lee el setup con getString /
// getNumber).
struct PropertySpec {
    std::string key;
    bool isNumber = false;
    std::string defaultText;         // valor al crear el objeto (si es de texto)
    double defaultNumber = 0.0;      // valor al crear el objeto (si es numero)
    std::vector<std::string> options; // texto: valores validos (vacio = texto libre)
};

struct ObjectTypeSpec {
    // Arma el objeto: le agrega componentes, tag, escala, sortingOrder...
    using SetupFn = std::function<void(GameObject* obj, const LevelObject& o)>;

    std::string type;
    std::string description;  // una linea de ayuda que muestra el editor
    SetupFn setup;            // vacio = el editor lo conoce, pero al jugar no se crea
    float w = 0.0f, h = 0.0f; // tamanio al crearlo, en pixeles del mapa (0 = punto)
    bool unique = false;      // a lo sumo UNO por nivel (el editor avisa si hay mas)
    bool spawn = false;       // es donde aparece el jugador (ver playerSpawn)
    std::vector<PropertySpec> properties;

    explicit ObjectTypeSpec(std::string type_, std::string description_ = "")
        : type(std::move(type_)), description(std::move(description_)) {}

    // Los metodos devuelven el mismo spec para poder encadenarlos al declararlo.

    // Propiedad de texto libre.
    ObjectTypeSpec& text(const std::string& key, const std::string& def = "") {
        PropertySpec p;
        p.key = key;
        p.defaultText = def;
        properties.push_back(p);
        return *this;
    }
    // Propiedad de texto con valores fijos; la primera opcion es la de por defecto.
    ObjectTypeSpec& choice(const std::string& key, const std::vector<std::string>& options) {
        PropertySpec p;
        p.key = key;
        p.options = options;
        if (!options.empty()) p.defaultText = options.front();
        properties.push_back(p);
        return *this;
    }
    // Propiedad numerica.
    ObjectTypeSpec& number(const std::string& key, double def = 0.0) {
        PropertySpec p;
        p.key = key;
        p.isNumber = true;
        p.defaultNumber = def;
        properties.push_back(p);
        return *this;
    }
    // Objeto con area (una zona): tamanio con el que nace, en pixeles del mapa.
    ObjectTypeSpec& size(float width, float height) {
        w = width;
        h = height;
        return *this;
    }
    // Solo puede haber uno por nivel (una meta, un jefe...).
    ObjectTypeSpec& single() {
        unique = true;
        return *this;
    }
    // Es el punto donde aparece el jugador. Implica single(). Con esto el editor puede
    // "jugar desde el cursor" (Shift+F2): juega una copia del nivel con este objeto
    // movido al raton, sin tocar el nivel.
    ObjectTypeSpec& playerSpawn() {
        spawn = unique = true;
        return *this;
    }

    const PropertySpec* findProperty(const std::string& key) const {
        for (const PropertySpec& p : properties)
            if (p.key == key) return &p;
        return nullptr;
    }
};

class ObjectCatalog {
public:
    // Agrega un type con la funcion que lo arma. Devuelve su spec para seguir
    // describiendolo (.choice, .number, .single, .playerSpawn...) en la misma linea.
    // No guardar esa referencia: el siguiente add puede moverla.
    ObjectTypeSpec& add(const std::string& type, ObjectTypeSpec::SetupFn setup,
                        const std::string& description = "") {
        types.emplace_back(type, description);
        types.back().setup = std::move(setup);
        return types.back();
    }

    // El spec de un type, o nullptr si el catalogo no lo tiene.
    const ObjectTypeSpec* find(const std::string& type) const {
        for (const ObjectTypeSpec& spec : types)
            if (spec.type == type) return &spec;
        return nullptr;
    }

    bool empty() const { return types.empty(); }
    void clear()       { types.clear(); }
    std::vector<ObjectTypeSpec>::const_iterator begin() const { return types.begin(); }
    std::vector<ObjectTypeSpec>::const_iterator end()   const { return types.end(); }

private:
    std::vector<ObjectTypeSpec> types;
};

// El spec de un type, o nullptr si el catalogo no lo tiene.
inline const ObjectTypeSpec* findObjectType(const ObjectCatalog& catalog, const std::string& type) {
    return catalog.find(type);
}
