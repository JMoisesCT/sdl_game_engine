#pragma once
#include <functional>
#include <string>
#include <vector>

#include "Component.h"
#include "GameObject.h"
#include "SpriteAnimator.h"

// Elige que animacion suena, a partir de REGLAS en vez de una cascada de if.
//
// Cada regla es una pareja (clip, condicion). En cada frame se recorren EN EL ORDEN EN
// QUE SE AGREGARON y gana la PRIMERA cuya condicion sea cierta; si no se cumple
// ninguna, suena el clip por defecto. O sea: el orden ES la prioridad.
//
// Por que existe: la logica "si esta en el aire subiendo -> jump, si esta en el aire ->
// fall, si se mueve -> run, si no -> idle" se repite en cada juego. Como reglas queda a
// la vista cual manda sobre cual, se pueden agregar estados desde sitios distintos y no
// hay que reordenar un bloque de if anidados para cambiar una prioridad.
//
//     auto fsm = player->addComponent<AnimatorStateMachine>();
//     fsm->setDefaultState("idle");
//     fsm->addState("jump", [motor] { return !motor->isGrounded() && motor->isRising(); });
//     fsm->addState("fall", [motor] { return !motor->isGrounded(); });
//     fsm->addState("run",  [motor] { return motor->moveInput != 0.0f; });
//
// Se agrega DESPUES del componente cuyo estado consultan las condiciones, para que lea
// datos de este frame y no del anterior. Llama a SpriteAnimator::play, que ya ignora el
// caso de "ya esta sonando", asi que evaluar cada frame no reinicia nada.

class AnimatorStateMachine : public Component {
public:
    // Agrega una regla. Las reglas mas especificas van PRIMERO.
    void addState(const std::string& clip, std::function<bool()> condition) {
        rules.push_back(Rule{ clip, std::move(condition) });
    }

    // Clip que suena cuando ninguna regla se cumple (tipicamente "idle").
    void setDefaultState(const std::string& clip) { defaultClip = clip; }

    // Nombre del clip que la maquina eligio en el ultimo frame (util para depurar).
    const std::string& getCurrentState() const { return currentClip; }

    void start() override {
        anim = gameObject->getComponent<SpriteAnimator>();
    }

    void update(float) override {
        if (!anim) return;
        const std::string* chosen = &defaultClip;
        for (const Rule& r : rules) {
            if (r.condition && r.condition()) { chosen = &r.clip; break; } // gana la primera
        }
        if (chosen->empty()) return;
        currentClip = *chosen;
        anim->play(currentClip);
    }

private:
    struct Rule {
        std::string clip;
        std::function<bool()> condition;
    };

    SpriteAnimator* anim = nullptr; // hermano, resuelto en start()
    std::vector<Rule> rules;
    std::string defaultClip;
    std::string currentClip;
};
