#include "Input.h"

#include <SDL3/SDL.h>

// Todo el estado vive aqui (no en el header): el alumno solo ve la API de consulta.
namespace {
    const int KEY_COUNT   = static_cast<int>(Key::Count);
    const int MOUSE_COUNT = static_cast<int>(MouseButton::Count);

    bool keysNow[KEY_COUNT]  = { false }; // estado de ESTE frame
    bool keysPrev[KEY_COUNT] = { false }; // estado del frame ANTERIOR

    bool mouseNow[MOUSE_COUNT]  = { false };
    bool mousePrev[MOUSE_COUNT] = { false };
    float cursorX = 0.0f, cursorY = 0.0f;

    // Traduccion Key -> scancode de SDL. Un switch (y no una tabla indexada por el
    // enum) para que agregar una tecla no dependa de mantener el orden del array.
    SDL_Scancode toScancode(Key k) {
        switch (k) {
        case Key::A: return SDL_SCANCODE_A;  case Key::B: return SDL_SCANCODE_B;
        case Key::C: return SDL_SCANCODE_C;  case Key::D: return SDL_SCANCODE_D;
        case Key::E: return SDL_SCANCODE_E;  case Key::F: return SDL_SCANCODE_F;
        case Key::G: return SDL_SCANCODE_G;  case Key::H: return SDL_SCANCODE_H;
        case Key::I: return SDL_SCANCODE_I;  case Key::J: return SDL_SCANCODE_J;
        case Key::K: return SDL_SCANCODE_K;  case Key::L: return SDL_SCANCODE_L;
        case Key::M: return SDL_SCANCODE_M;  case Key::N: return SDL_SCANCODE_N;
        case Key::O: return SDL_SCANCODE_O;  case Key::P: return SDL_SCANCODE_P;
        case Key::Q: return SDL_SCANCODE_Q;  case Key::R: return SDL_SCANCODE_R;
        case Key::S: return SDL_SCANCODE_S;  case Key::T: return SDL_SCANCODE_T;
        case Key::U: return SDL_SCANCODE_U;  case Key::V: return SDL_SCANCODE_V;
        case Key::W: return SDL_SCANCODE_W;  case Key::X: return SDL_SCANCODE_X;
        case Key::Y: return SDL_SCANCODE_Y;  case Key::Z: return SDL_SCANCODE_Z;

        case Key::Num0: return SDL_SCANCODE_0;  case Key::Num1: return SDL_SCANCODE_1;
        case Key::Num2: return SDL_SCANCODE_2;  case Key::Num3: return SDL_SCANCODE_3;
        case Key::Num4: return SDL_SCANCODE_4;  case Key::Num5: return SDL_SCANCODE_5;
        case Key::Num6: return SDL_SCANCODE_6;  case Key::Num7: return SDL_SCANCODE_7;
        case Key::Num8: return SDL_SCANCODE_8;  case Key::Num9: return SDL_SCANCODE_9;

        case Key::Left:  return SDL_SCANCODE_LEFT;
        case Key::Right: return SDL_SCANCODE_RIGHT;
        case Key::Up:    return SDL_SCANCODE_UP;
        case Key::Down:  return SDL_SCANCODE_DOWN;

        case Key::Space:     return SDL_SCANCODE_SPACE;
        case Key::Enter:     return SDL_SCANCODE_RETURN;
        case Key::Escape:    return SDL_SCANCODE_ESCAPE;
        case Key::Tab:       return SDL_SCANCODE_TAB;
        case Key::Backspace: return SDL_SCANCODE_BACKSPACE;

        case Key::LShift: return SDL_SCANCODE_LSHIFT;
        case Key::RShift: return SDL_SCANCODE_RSHIFT;
        case Key::LCtrl:  return SDL_SCANCODE_LCTRL;
        case Key::RCtrl:  return SDL_SCANCODE_RCTRL;
        case Key::LAlt:   return SDL_SCANCODE_LALT;
        case Key::RAlt:   return SDL_SCANCODE_RALT;

        case Key::F1:  return SDL_SCANCODE_F1;   case Key::F2:  return SDL_SCANCODE_F2;
        case Key::F3:  return SDL_SCANCODE_F3;   case Key::F4:  return SDL_SCANCODE_F4;
        case Key::F5:  return SDL_SCANCODE_F5;   case Key::F6:  return SDL_SCANCODE_F6;
        case Key::F7:  return SDL_SCANCODE_F7;   case Key::F8:  return SDL_SCANCODE_F8;
        case Key::F9:  return SDL_SCANCODE_F9;   case Key::F10: return SDL_SCANCODE_F10;
        case Key::F11: return SDL_SCANCODE_F11;  case Key::F12: return SDL_SCANCODE_F12;

        default: return SDL_SCANCODE_UNKNOWN;
        }
    }

    // Lee el hardware y vuelca el estado ACTUAL en keysNow / mouseNow / cursor.
    void readDevices() {
        int count = 0;
        const bool* sdlKeys = SDL_GetKeyboardState(&count); // SDL3: const bool*
        for (int i = 0; i < KEY_COUNT; ++i) {
            SDL_Scancode sc = toScancode(static_cast<Key>(i));
            keysNow[i] = (sdlKeys && sc != SDL_SCANCODE_UNKNOWN && sc < count)
                       ? sdlKeys[sc] : false;
        }

        SDL_MouseButtonFlags buttons = SDL_GetMouseState(&cursorX, &cursorY);
        mouseNow[static_cast<int>(MouseButton::Left)]   = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))   != 0;
        mouseNow[static_cast<int>(MouseButton::Middle)] = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0;
        mouseNow[static_cast<int>(MouseButton::Right)]  = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))  != 0;
    }

    // Consulta segura: fuera de rango responde "sin pulsar" en vez de leer basura.
    bool valid(Key k)         { int i = static_cast<int>(k); return i >= 0 && i < KEY_COUNT; }
    bool valid(MouseButton b) { int i = static_cast<int>(b); return i >= 0 && i < MOUSE_COUNT; }
}

void Input::update() {
    // El estado de este frame pasa a ser el del frame anterior, y releemos.
    for (int i = 0; i < KEY_COUNT; ++i)   keysPrev[i]  = keysNow[i];
    for (int i = 0; i < MOUSE_COUNT; ++i) mousePrev[i] = mouseNow[i];
    readDevices();
}

void Input::reset() {
    // Releemos y dejamos "anterior" == "actual": no hay flancos pendientes, pero las
    // teclas que el jugador tiene apretadas siguen contando como mantenidas.
    readDevices();
    for (int i = 0; i < KEY_COUNT; ++i)   keysPrev[i]  = keysNow[i];
    for (int i = 0; i < MOUSE_COUNT; ++i) mousePrev[i] = mouseNow[i];
}

bool Input::isDown(Key k)      { return valid(k) &&  keysNow[(int)k]; }
bool Input::wasPressed(Key k)  { return valid(k) &&  keysNow[(int)k] && !keysPrev[(int)k]; }
bool Input::wasReleased(Key k) { return valid(k) && !keysNow[(int)k] &&  keysPrev[(int)k]; }

float Input::axis(Key negative, Key positive) {
    float v = 0.0f;
    if (isDown(negative)) v -= 1.0f;
    if (isDown(positive)) v += 1.0f;
    return v; // ambas a la vez se cancelan: 0
}

bool Input::isMouseDown(MouseButton b)      { return valid(b) &&  mouseNow[(int)b]; }
bool Input::wasMousePressed(MouseButton b)  { return valid(b) &&  mouseNow[(int)b] && !mousePrev[(int)b]; }
bool Input::wasMouseReleased(MouseButton b) { return valid(b) && !mouseNow[(int)b] &&  mousePrev[(int)b]; }

float Input::mouseX() { return cursorX; }
float Input::mouseY() { return cursorY; }
