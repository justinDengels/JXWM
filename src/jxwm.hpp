#pragma once

#include "logger.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <vector>
#include <array>
#include <string>



typedef struct
{
    int x;
    int y;
    uint w;
    uint h;
} Rect;

typedef struct Client
{
    Window window;
    bool isFullScreen;
    Rect og;
} Client;

typedef struct
{
    long left;
    long right;
    long top;
    long bottom;
} Strut;

typedef union
{
    const char* spawn; //wish to make this a std::string if possible
    int tag;
} arg;

typedef struct 
{
    int borderWidth;
    unsigned int focusedBorderColor;
    unsigned int unfocusedBorderColor;
    bool switchOnOpen;
    bool switchOnMove;
} Settings;

class JXWM
{
public:
    JXWM();
    ~JXWM();
    int Init();
    void Run();

private:
    Logger logger;
    Window root;
    Display* disp;
    int screenum;
    Settings settings; 
    Rect screenArea;
    Rect usableArea;
    static bool otherWM;
    bool quit;
    int tags;
    size_t currentTag;
    static int OnOtherWMDetected(Display* disp, XErrorEvent* xee);
    static int OnError(Display* disp, XErrorEvent* xee);
    void GetAtoms();
    void ReadConfigFile(const std::string& configFile);

    Client* focused;
    std::array<std::vector<Client>, 9> Clients;
    Client* GetClientFromWindow(Window w);
    void RemoveClient(Client* c);
    int GetClientTag(Client* c);

    void GetExistingWindows();


    typedef struct
    {
        unsigned mod;
        KeySym sym;
        void (JXWM::*func)(arg*);
        arg argument;
    } keybinding;
    std::vector<keybinding> keybindings;

    void GrabKeys();

    void (JXWM::*handlers[LASTEvent])(const XEvent&);
    void GrabHandlers();
    void OnMapRequest(const XEvent& e);
    void OnUnmapNotify(const XEvent& e);
    void OnConfigureRequest(const XEvent& e);
    void OnKeyPress(const XEvent& e);
    void OnClientMessage(const XEvent& e);
    void OnDestroyNotify(const XEvent& e);
    void OnWindowEnter(const XEvent& e);
    void OnCreateNotify(const XEvent& e);

    const char* EventToString(int event);

    void Spawn(arg* arg);
    void Spawn(const char* spawn);
    void KillWindow(arg* arg);
    void KillWindow(Client* c);
    void ChangeTag(arg* arg);
    void ChangeTag(int tagToChange);
    void ChangeClientTag(arg* arg);
    void ChangeClientTag(Client* c, int tag);
    void Quit(arg* arg);
    void Logout(arg* arg);
    void ReloadConfig(arg* arg);
    void MoveClientLeft(arg* arg);
    void MoveClientRight(arg* arg);
    void MoveClient(Client* c, int amount);

    void FocusClient(Client* c);
    void FullscreenClient(Client* c, bool toggle);

    enum 
    { 
        WM_DELETE_WINDOW, 
        WM_PROTOCOLS, 
        NET_SUPPORTED, 
        NET_ACTIVE_WINDOW, 
        NET_NUMBER_OF_DESKTOPS, 
        NET_CURRENT_DESKTOP, 
        NET_CLOSE_WINDOW, 
        NET_WM_STRUT_PARTIAL, 
        NET_WM_STATE,
        NET_WM_STATE_FULLSCREEN,
        NUM_ATOMS
    };

    Atom atoms[NUM_ATOMS];

    bool IsPager(Window w, Strut& strutsRet);
    void UpdateStruts(Strut& struts);
    void Arrange();
    void (JXWM::*layout)(void);
    void SetWindowLayout(void(JXWM::*func)(void));
    //Same as the XMoveResizeWindow function but takes into account the border width
    int JMoveResizeClient(Client& c, int x, int y, uint w, uint h);
    void MasterStack();

    bool WindowHasAtom(Window w, Atom atom);
};
