#pragma once

struct GLFWwindow;

struct AppEvent
{
    enum Type {
        WindowClose,
        WindowFocus,
        WindowIconify,
        WindowSize,
        KeyPress,
        CursorPos,
        MouseButton,
        Scroll,
        CharInput,
    };

    AppEvent(AppEvent::Type t, GLFWwindow* w) : type(t), window(w) {}

    struct WindowCloseEvent
    {

    };

    struct WindowFocusEvent
    {
        int focused;
    };

    struct WindowIconifyEvent
    {
        int iconified;
    };

    struct WindowSizeEvent
    {
        int width;
        int height;
    };

    struct KeyPressEvent
    {
        int key;
        int scancode;
        int action;
        int mods;
    };

    struct CursorPosEvent
    {
        double xpos;
        double ypos;
    };

    struct MouseButtonEvent
    {
        int button;
        int action;
        int mods;
    };

    struct ScrollEvent
    {
        double xoffset;
        double yoffset;
    };

    struct CharInputEvent
    {
        unsigned int c;
    };

    union {
        WindowCloseEvent windowClose;
        WindowFocusEvent windowFocus;
        WindowIconifyEvent windowIconify;
        WindowSizeEvent windowSize;
        KeyPressEvent keyPress;
        CursorPosEvent cursorPos;
        MouseButtonEvent mouseButton;
        ScrollEvent scroll;
        CharInputEvent charInput;
    };
    GLFWwindow* window;
    Type type;
};