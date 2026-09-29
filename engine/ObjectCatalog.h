#pragma once
#include <string>
#include <utility>
#include <vector>

// CATALOGO DE OBJETOS: la lista de "type" que entiende la fabrica de UN juego, con sus
// propiedades y valores por defecto. Lo escribe el JUEGO (al lado de su fabrica) y se lo
// pasa al editor de niveles. El motor sigue sin conocer ningun type, pero con el
// catalogo el editor puede:
//   - ofrecer "Nuevo -> Spikes" aunque el nivel todavia no tenga ninguno,
//   - crear cada objeto con sus propiedades por defecto ya puestas,
//   - mostrar un combo con los valores validos (las frutas) en vez de texto libre,
//   - avisar de un type o un valor mal escrito ("Aple") ANTES de jugar,
//   - y saber que type es el punto de aparicion del jugador ("Jugar desde aqui").
//
//     ObjectCatalog catalog;
//     catalog.push_back(ObjectTypeSpec("PlayerStart", "Donde aparece el jugador")
//                           .playerSpawn());
//     catalog.push_back(ObjectTypeSpec("Fruit", "Fruta que se recoge")
//                           .choice("fruit", { "Apple", "Bananas", "Cherries" }));
//     catalog.push_back(ObjectTypeSpec("Spikes", "Pinchos: quitan vida al tocarlos"));
//
// Es OPCIONAL: sin catalogo, el editor ofrece los type que ya hay en el nivel y deja
// escribir cualquiera, como antes.
//
// Ojo: el catalogo y la fabrica tienen que decir lo mismo. Si la fabrica aprende un type
// nuevo, se agrega aqui tambien; un type del catalogo sin fabrica se puede crear en el
// editor, pero no aparece al jugar.

// Una propiedad que el juego espera en un type (la lee la fabrica con getString /
// getNumber).
struct PropertySpec {
    std::string key;
    bool isNumber = false;
    std::string defaultText;         // valor al crear el objeto (si es de texto)
    double defaultNumber = 0.0;      // valor al crear el objeto (si es numero)
    std::vector<std::string> options; // texto: valores validos (vacio = texto libre)
};

struct ObjectTypeSpec {
    std::string type;
    std::string description;  // una linea de ayuda que muestra el editor
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
    // Objeto con area (una zona): tamanio con el que nace.
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

using ObjectCatalog = std::vector<ObjectTypeSpec>;

// El spec de un type, o nullptr si el catalogo no lo tiene.
inline const ObjectTypeSpec* findObjectType(const ObjectCatalog& catalog, const std::string& type) {
    for (const ObjectTypeSpec& spec : catalog)
        if (spec.type == type) return &spec;
    return nullptr;
}
