#pragma once

// Estado del teclado y del raton, consultable desde CUALQUIER componente.
//
// Por que existe: SDL_GetKeyboardState solo dice si una tecla esta MANTENIDA. El
// "se acaba de presionar" (flanco) vivia en el bucle de eventos de main, inalcanzable
// desde un componente, asi que cada controlador se inventaba su propia memoria del
// frame anterior (un bool jumpPrev por componente). Input guarda el estado de ESTE
// frame y del ANTERIOR una sola vez, y todos consultan lo mismo.
//
// No es un componente: es una clase ESTATICA (un unico estado global de entrada).
// Tampoco incluye SDL: las teclas se nombran con el enum Key de abajo y la traduccion
// a scancodes de SDL ocurre solo en Input.cpp.
//
// Uso desde el bucle de main, UNA vez por frame, ANTES de scene->update(dt):
//     Input::update();
// Uso desde un componente:
//     if (Input::wasPressed(Key::Space)) saltar();
//     float mover = Input::axis(Key::Left, Key::Right);   // -1, 0 o +1

// Teclas disponibles. Se pueden agregar mas: basta sumar el valor aqui y su caso en
// la traduccion de Input.cpp (el compilador avisa si falta, la tabla es exhaustiva).
enum class Key {
    // Letras
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Numeros de la fila superior
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    // Flechas
    Left, Right, Up, Down,
    // Teclas de control
    Space, Enter, Escape, Tab, Backspace,
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,
    // Funcion
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    Count // marcador final: cuantas teclas maneja Input (no es una tecla)
};

enum class MouseButton { Left, Middle, Right, Count };

class Input {
public:
    // Refresca el estado. La llama el bucle de main una vez por frame, antes de
    // actualizar la escena. Guarda el estado del frame previo para los flancos.
    static void update();

    // Olvida los flancos sin perder que teclas estan mantenidas: deja el estado
    // "previo" igual al actual. Llamarlo al cambiar de escena, para que la tecla que
    // provoco el cambio no se lea como recien presionada en la escena nueva.
    static void reset();

    static bool isDown(Key k);       // mantenida en este frame
    static bool wasPressed(Key k);   // paso de suelta a mantenida en este frame
    static bool wasReleased(Key k);  // paso de mantenida a suelta en este frame

    // Eje digital: +1 si solo esta 'positive', -1 si solo 'negative', 0 si ninguna
    // o ambas. Atajo del patron "if (izquierda) -1; if (derecha) +1".
    static float axis(Key negative, Key positive);

    static bool isMouseDown(MouseButton b);
    static bool wasMousePressed(MouseButton b);
    static bool wasMouseReleased(MouseButton b);

    // Posicion del cursor en pixeles de PANTALLA (no de mundo): la esquina superior
    // izquierda de la ventana es (0,0). Para llevarlo al mundo hace falta la camara.
    static float mouseX();
    static float mouseY();
};
