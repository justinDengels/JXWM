#include "jxwm.hpp"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <algorithm>

//#define PRINTHERE std::cout << __LINE__ << std::endl;
#define PRINTHERE logger.Log(DEBUG, std::to_string(__LINE__));
bool JXWM::otherWM = false;


JXWM::JXWM()
    : disp(nullptr), focused(nullptr), currentTag(0), tags(9), logger("jxwm.log")
{}

JXWM::~JXWM()
{
    logger.Log(INFO, "Quiting JXWM...");
    XCloseDisplay(disp);
    //pkill -KILL -u $USER
}

int JXWM::Init()
{
    disp = XOpenDisplay(NULL);
    if (disp == NULL)
    {
        logger.Log(ERROR, "Failed to open X Display");
        return 1;
    }

    root = DefaultRootWindow(disp);
    XSetErrorHandler(&OnOtherWMDetected);
    long rootMask = SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | PropertyChangeMask;
    XSelectInput(disp, root, rootMask);
    SetWindowLayout(&JXWM::MasterStack);
    GetAtoms();
    GetExistingWindows();
    GrabHandlers();
    const char* HOME = std::getenv("HOME");
    std::string configPath = std::string(HOME) + "/.config/jxwm/jxwm.conf";
    ReadConfigFile(configPath);
    GrabKeys();
    XSync(disp, false);
    if (otherWM) 
    {
        logger.Log(ERROR, "Other window manager detected");
        return 1; 
    }

    XSetErrorHandler(&OnError);
    quit = false;
    screenArea.x = screenArea.y = usableArea.x = usableArea.y = 0;
    screenum = DefaultScreen(disp);
    screenArea.w = usableArea.w = DisplayWidth(disp, screenum);
    screenArea.h = usableArea.h = DisplayHeight(disp, screenum);
    return 0;
}

void JXWM::GetAtoms()
{
    wmAtoms[WM_PROTOCOLS] = XInternAtom(disp, "WM_PROTOCOLS", false);
    wmAtoms[WM_DELETE_WINDOW] = XInternAtom(disp, "WM_DELETE_WINDOW", false);
    wmAtoms[WM_STATE] = XInternAtom(disp, "WM_STATE", false);
    wmAtoms[UTF8_STRING] = XInternAtom(disp, "UTF8_STRING", false);
    netAtoms[NET_ACTIVE_WINDOW] = XInternAtom(disp, "_NET_ACTIVE_WINDOW", false);
    netAtoms[NET_CLOSE_WINDOW] = XInternAtom(disp, "_NET_CLOSE_WINDOW", false);
    netAtoms[NET_SUPPORTED] = XInternAtom(disp, "_NET_SUPPORTED", false);
    netAtoms[NET_CLIENT_LIST] = XInternAtom(disp, "_NET_CLIENT_LIST", false);
    netAtoms[NET_WM_STRUT_PARTIAL] = XInternAtom(disp, "_NET_WM_STRUT_PARTIAL", false);
    netAtoms[NET_WM_STATE] = XInternAtom(disp, "_NET_WM_STATE", false);
    netAtoms[NET_WM_STATE_FULLSCREEN] = XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", false);
    netAtoms[NET_SUPPORTING_WM_CHECK] = XInternAtom(disp, "_NET_SUPPORTING_WM_CHECK", false);
    netAtoms[NET_WM_NAME] = XInternAtom(disp, "_NET_WM_NAME", false);

    netAtoms[NET_NUMBER_OF_DESKTOPS] = XInternAtom(disp, "_NET_NUMBER_OF_DESKTOPS", false);
    XChangeProperty(disp, root, netAtoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);

    netAtoms[NET_CURRENT_DESKTOP] = XInternAtom(disp, "_NET_CURRENT_DESKTOP", false);
    XChangeProperty(disp, root, netAtoms[NET_CURRENT_DESKTOP], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&currentTag, 1);

    wmCheck = XCreateSimpleWindow(disp, root, 0, 0, 1, 1, 0, 0, 0);

    XChangeProperty(disp, root, netAtoms[NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&wmCheck, 1);
    XChangeProperty(disp, wmCheck, netAtoms[NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32, PropModeReplace, (unsigned char*)&wmCheck, 1);

    const char* name = "jxwm";
    XChangeProperty(disp, wmCheck, netAtoms[NET_WM_NAME], wmAtoms[UTF8_STRING], 8, PropModeReplace, (unsigned char*)name, strlen(name));

    XChangeProperty(disp, root, netAtoms[NET_SUPPORTED], XA_ATOM, 32, PropModeReplace, (unsigned char *)netAtoms, sizeof(netAtoms) / sizeof(Atom));
}

void JXWM::GetExistingWindows()
{
    XGrabServer(disp);
    Window rootr, pReturn, *children;
    uint nChildren;
    XQueryTree(disp, root, &rootr, &pReturn, &children, &nChildren);
    for (uint i = 0; i < nChildren; i++) 
    {  
        if (children[i] != wmCheck) { Clients[currentTag].push_back({children[i]}); } 
    }
    XUngrabServer(disp);
    XFree(children);
    Arrange();
}

void JXWM::Run()
{
    XEvent e;
    while (!quit)
    {
        XNextEvent(disp, &e);
        logger.Log(DEBUG, "Got event " + std::string(EventToString(e.type)));
        if (handlers[e.type]) { (this->*handlers[e.type])(e); }
    }
}

int JXWM::OnOtherWMDetected(Display* disp, XErrorEvent* xee)
{
    if ((int)xee->error_code == BadAccess) { otherWM = true; }
    return 0;
}

int JXWM::OnError(Display* disp, XErrorEvent* xee)
{
    std::cout << "An error occurred" << std::endl;
    char text[512];
    XGetErrorText(disp, xee->error_code, text, sizeof(text));
    std::stringstream ss;
    ss << text << "\nCode: " << xee->error_code
        << "\nType: " << xee->type << "\nSerial: " << xee->serial
        << "\nMinor: " << xee->minor_code << "\nResource ID: " << xee->resourceid
        << "\nRequest Code: " << xee->request_code;
    std::cout << ss.str() << std::endl;
    return 0;
}

void JXWM::GrabKeys()
{
    XUngrabKey(disp, AnyKey, AnyModifier, root);
    //first, grab tag keybindings
    for (int i = 0; i < 9; i++)
    {
        keybindings.push_back({Mod4Mask | ShiftMask, XK_1 + (KeySym)i, &JXWM::ChangeClientTag, {.tag = i}});
        keybindings.push_back({Mod4Mask, XK_1 + (KeySym)i, &JXWM::ChangeTag, {.tag = i}});
    }

    for (auto kb: keybindings)
    {
        KeyCode kc = XKeysymToKeycode(disp, kb.sym);
        if (kc) { XGrabKey(disp, kc, kb.mod, root, True, GrabModeAsync, GrabModeAsync); }
    }
}

void JXWM::GrabHandlers()
{
    handlers[MapRequest] = &JXWM::OnMapRequest;
    handlers[UnmapNotify] = &JXWM::OnUnmapNotify;
    handlers[ConfigureRequest] = &JXWM::OnConfigureRequest;
    handlers[KeyPress] = &JXWM::OnKeyPress;
    handlers[ClientMessage] = &JXWM::OnClientMessage;
    handlers[DestroyNotify] = &JXWM::OnDestroyNotify;
    handlers[EnterNotify] = &JXWM::OnWindowEnter;
    handlers[LeaveNotify] = &JXWM::OnWindowLeave;
    handlers[CreateNotify] = &JXWM::OnCreateNotify;
    handlers[PropertyNotify] = &JXWM::OnPropertyNotify;
}

static const char* eventNames[] =
{
    "",
    "",
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify"
};

const char* JXWM::EventToString(int event)
{
    return eventNames[event];
}

void JXWM::OnMapRequest(const XEvent& e)
{
    Window w = e.xmaprequest.window;
    Client* c = GetClientFromWindow(w);
    if (c != nullptr)
    {
        int cTag = GetClientTag(c);
        if (currentTag != cTag) 
        {
            if (settings.switchOnOpen) { ChangeTag(cTag); }
            else { return; }
        }
        else 
        {
            XMapWindow(disp, c->window);
            Arrange();
        }
        return;
    }

    static XWindowAttributes wa;

    if (!XGetWindowAttributes(disp, w, &wa)) { return; }

    if (wa.override_redirect)
    {
        XMapWindow(disp, w);
        return;
    }

    static Strut struts;

    if (IsPager(w, struts))
    {
        UpdateStruts(struts);
        XMapWindow(disp, w);
        Arrange();
        return;
    }

    XMapWindow(disp, w);
    XSelectInput(disp, w, EnterWindowMask | LeaveWindowMask | PropertyChangeMask);
    XSetWindowBorderWidth(disp, w, settings.borderWidth);
    Client newc;
    newc.window  = w;
    Status ok = XFetchName(disp, w, &newc.title);
    if (!ok) { newc.title = (char*)"NULL"; }
    newc.isFullScreen = False;
    Clients[currentTag].push_back(newc);
    FocusClient(&newc);
    Arrange();
    UpdateClientList();
}

void JXWM::OnConfigureRequest(const XEvent& e)
{
    XConfigureRequestEvent xcre = e.xconfigurerequest;
    XWindowChanges xwc;
    xwc.x = xcre.x;
    xwc.y = xcre.y;
    xwc.width = xcre.width;
    xwc.height = xcre.height;
    xwc.sibling = xcre.above;
    xwc.stack_mode = xcre.detail;
    xwc.border_width = xcre.border_width;
    XConfigureWindow(disp, xcre.window, xcre.value_mask, &xwc);
}

void JXWM::OnKeyPress(const XEvent& e)
{
    XKeyEvent xke = e.xkey;

    for (auto kb: keybindings)
    {
        if ((xke.keycode == XKeysymToKeycode(disp, kb.sym)) && ((kb.mod & xke.state) == kb.mod))
        {
            (this->*kb.func)(&kb.argument);
        }
    }
}

void JXWM::Spawn(arg* arg) { Spawn(arg->spawn); }

void JXWM::Spawn(const char* spawn)
{
    logger.Log(DEBUG, "In Spawn function");
    if (fork() == 0)
    {
        if (disp) { close(ConnectionNumber(disp)); }
        setsid();
        execl("/bin/sh", "sh", "-c", spawn, 0);
        exit(0);
    }
}

void JXWM::KillWindow(arg* arg) { KillWindow(focused); }

void JXWM::KillWindow(Client* c)
{
    logger.Log(DEBUG, "In KillWindow Function");
    if (c == nullptr) { return; }
    if (WindowHasAtom(c->window, wmAtoms[WM_DELETE_WINDOW]))
    {
        XEvent killEvent;
        memset(&killEvent, 0, sizeof(killEvent));
        killEvent.xclient.type = ClientMessage;
        killEvent.xclient.window = c->window;
        killEvent.xclient.message_type = wmAtoms[WM_PROTOCOLS];
        killEvent.xclient.format = 32;
        killEvent.xclient.data.l[0] = wmAtoms[WM_DELETE_WINDOW];
        killEvent.xclient.data.l[1] = CurrentTime;
        XSendEvent(disp, c->window, False, NoEventMask, &killEvent);
        RemoveClient(c);
        Arrange();
        return;
    }
    logger.Log(ERROR, "Could not get delete window atom");
    XKillClient(disp, c->window);
    RemoveClient(c);
    Arrange();
}

bool JXWM::WindowHasAtom(Window w, Atom atom)
{
    Atom* retAtoms;
    int numAtoms;

    if (XGetWMProtocols(disp, w, &retAtoms, &numAtoms))
    {
        for (int i = 0; i < numAtoms; i++)
        {
            if (retAtoms[i] == atom)
            {
                XFree(retAtoms);
                return True;
            }
        }
        XFree(retAtoms);
    }
    return false;
}


void JXWM::OnClientMessage(const XEvent& e)
{
    XClientMessageEvent xcme = e.xclient;

    Client* c = GetClientFromWindow(xcme.window);
    std::string title = (!c) ? "UNKNOWN CLIENT" : std::string(c->title);
logger.Log(INFO, "Got client message " + std::string(XGetAtomName(disp, xcme.message_type)) + " from client " + title); 
    if (xcme.message_type == netAtoms[NET_NUMBER_OF_DESKTOPS])
    {
        tags = (int)xcme.data.l[0];
        XChangeProperty(disp, root, netAtoms[NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&tags, 1);
        logger.Log(INFO, "Got client message to change number of virtual desktops to " + std::to_string(tags));
    }
    else if (xcme.message_type == netAtoms[NET_CURRENT_DESKTOP])
    {
        int clientTag = (int)xcme.data.l[0];
        XChangeProperty(disp, root, netAtoms[NET_CURRENT_DESKTOP], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&clientTag, 1);
        logger.Log(INFO, "Got client message to change current tag to tag " + std::to_string(currentTag));
        ChangeTag(clientTag);
    }
    else if (xcme.message_type == netAtoms[NET_CLOSE_WINDOW])
    {
        logger.Log(INFO, "Got client message to kill a window, attempting to kill it");
        Client* c = GetClientFromWindow(xcme.window);
        KillWindow(c);
    }
    else if (xcme.message_type == netAtoms[NET_WM_STATE])
    {
        long action = xcme.data.l[0];
        Atom prop1 = xcme.data.l[1];
        //Atom prop2 = xcme.data.l[2];
        logger.Log(INFO,"Window: " /*+ title +*/ "\n_NET_WM_STATE:\nAction: " + std::to_string(action) + "\nAtom1: "
                + std::string(XGetAtomName(disp, prop1))); //+ "\nAtom2: " + 
                //std::string(XGetAtomName(disp, prop2)));
        if (prop1 == netAtoms[NET_WM_STATE_FULLSCREEN])
        {
            if (!c) { return; }
            if (action == 0) { FullscreenClient(c, false); }
            else if (action == 1) { FullscreenClient(c, true); }
            else if (action == 2) { FullscreenClient(c, !c->isFullScreen); }
        }
    }
    else if (xcme.message_type == netAtoms[NET_ACTIVE_WINDOW])
    {
        if (c != nullptr) { FocusClient(c); }
    }
}

void JXWM::OnDestroyNotify(const XEvent& e)
{
    XDestroyWindowEvent xdwe = e.xdestroywindow;
    Client* c = GetClientFromWindow(xdwe.window);
    if (c == nullptr) { return; }
    RemoveClient(c);
}

void JXWM::FocusClient(Client* c)
{
    if (c == nullptr || c == focused) { return; }
    if (focused != nullptr) { XSetWindowBorder(disp, focused->window, settings.unfocusedBorderColor); }
    focused = c;
    XSetWindowBorder(disp, c->window, settings.focusedBorderColor);
    XSetInputFocus(disp, c->window, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(disp, c->window);
    XChangeProperty(disp, root, netAtoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&c->window, 1);
}

Client* JXWM::GetClientFromWindow(Window w)
{
    for (auto i = 0; i < Clients.size(); i++)
    {
        for (auto j = 0; j < Clients[i].size(); j++)
        {
            if (Clients[i][j].window == w) { return &Clients[i][j]; }
        }
    }
    return nullptr;
}

void JXWM::RemoveClient(Client* c)
{
    if (c == nullptr) { return; }

    for (auto i = 0; i < Clients.size(); i++)
    {
        for (auto it = Clients[i].begin(); it != Clients[i].end(); it++)
        {
            if (it->window == c->window)
            {
                if (c->window == focused->window) { focused = nullptr; }
                Clients[i].erase(it);
                Arrange();
                UpdateClientList();
                return;
            }
        }
    }
}


void JXWM::MasterStack()
{
    std::size_t numClients = Clients[currentTag].size();
    if (numClients == 0) { return; }
    if (numClients == 1)
    {
        if (!Clients[currentTag][0].isFullScreen) 
        {
            JMoveResizeClient(Clients[currentTag][0], usableArea.x, usableArea.y, usableArea.w, usableArea.h);
        }
        return;
    }

    Client master = Clients[currentTag][0];
    if (!Clients[currentTag][0].isFullScreen)
    {
        JMoveResizeClient(master, usableArea.x, usableArea.y, usableArea.w/2, usableArea.h);
    }
    int yGap = usableArea.h / (numClients - 1);
    int yOffSet = usableArea.y;
    for (std::size_t i = 1; i < numClients; i++)
    {
        if (Clients[currentTag][i].isFullScreen) { continue; }
        JMoveResizeClient(Clients[currentTag][i], usableArea.w / 2, yOffSet, usableArea.w / 2, yGap);
        yOffSet += yGap;
    }

}

void JXWM::SetWindowLayout(void(JXWM::*func)(void)) { layout = func; }

void JXWM::Arrange() { (this->*layout)(); }

void JXWM::OnWindowEnter(const XEvent& e)
{
    if ((e.xcrossing.mode != NotifyNormal) || (e.xcrossing.detail == NotifyInferior)) { return; }
    Client* c = GetClientFromWindow(e.xcrossing.window);
    FocusClient(c); 
}

void JXWM::OnWindowLeave(const XEvent& e) 
{

}

void JXWM::OnUnmapNotify(const XEvent& e)
{

}

void JXWM::ChangeTag(arg* arg) { ChangeTag(arg->tag); }

void JXWM::ChangeTag(int tagToChange)
{

    if (currentTag == tagToChange) { return; }

    for (auto c: Clients[currentTag]) { XUnmapWindow(disp, c.window); }
    currentTag = tagToChange;
    for (auto c: Clients[currentTag]) { XMapWindow(disp, c.window); }

    logger.Log(INFO, "Changed to tag " + std::to_string(currentTag)); 
    XChangeProperty(disp, root, netAtoms[NET_CURRENT_DESKTOP], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&currentTag, 1);
    Arrange();
}

void JXWM::OnCreateNotify(const XEvent& e)
{
}

void JXWM::Quit(arg* arg)
{
    quit = true;
}

void JXWM::ReloadConfig(arg* arg)
{
    logger.Log(INFO, "Reloading config...");
    keybindings.clear();
    const char* HOME = std::getenv("HOME");
    std::string configPath = std::string(HOME) + "/.config/jxwm/jxwm.conf";
    ReadConfigFile(configPath);
    GrabKeys();
}

bool JXWM::IsPager(Window w, Strut& strutsRet)
{
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    long* prop = nullptr;

    if (XGetWindowProperty(disp, w, netAtoms[NET_WM_STRUT_PARTIAL], 0, 12, False, XA_CARDINAL, &actualType, &actualFormat, &nItems, &bytesAfter, (unsigned char**)&prop) == Success && prop)
   {
       if (nItems == 12)
       {
           strutsRet.left = prop[0];
           strutsRet.right = prop[1];
           strutsRet.top = prop[2];
           strutsRet.bottom = prop[3];
           XFree(prop);
           return true;
       }
        XFree(prop);
       return true;
   }
    else { return false; }
}

void JXWM::UpdateStruts(Strut& struts)
{
    usableArea.x = struts.left;
    usableArea.y = struts.top;
    usableArea.w = screenArea.w - struts.left - struts.right;
    usableArea.h = screenArea.h - struts.top - struts.bottom;
    logger.Log(INFO, "Updated workable area");
    logger.Log(DEBUG, "Old Area: (x,y,w,h)  " + std::to_string(screenArea.x) + " " + std::to_string(screenArea.y) + " " + std::to_string(screenArea.w) + " " + std::to_string(screenArea.h));
    logger.Log(DEBUG, "New Area: (x,y,w,h)  " + std::to_string(usableArea.x) + " " + std::to_string(usableArea.y) + " " + std::to_string(usableArea.w) + " " + std::to_string(usableArea.h));
}

void JXWM::ChangeClientTag(arg* arg) { ChangeClientTag(focused, arg->tag); }

void JXWM::ChangeClientTag(Client* c, int tag)
{
    if (c == nullptr || currentTag == tag || tag < 0 || tag >= tags) { return; }
    auto& from = Clients[currentTag];
    auto& to = Clients[tag];
    for (auto it = from.begin(); it != from.end(); it++)
    {
        if (it->window == c->window)
        {
            to.push_back(std::move(*it));
            from.erase(it);
            break;
        }
    }

    if (settings.switchOnMove) { ChangeTag(tag); }
}

int JXWM::JMoveResizeClient(Client& c, int x, int y, uint w, uint h)
{
    return XMoveResizeWindow(disp, c.window, x, y, w - (2 * settings.borderWidth), h - (2 * settings.borderWidth));
}

void JXWM::MoveClientLeft(arg* arg) { MoveClient(focused, -1); }

void JXWM::MoveClientRight(arg* arg) { MoveClient(focused, 1); }

void JXWM::MoveClient(Client* c, int amount)
{
    if (c == nullptr || Clients[currentTag].size() <= 1) { return; }
    int idx;
    for (auto i = 0; i < Clients[currentTag].size(); i++)
    {
        if (Clients[currentTag][i].window == c->window)
        {
            idx = i;
            break;
        }
    }
    int other = (idx + amount) % Clients[currentTag].size();
    std::swap(Clients[currentTag][idx], Clients[currentTag][other]);
    Arrange();
}

void JXWM::Logout(arg* arg) { Spawn("pkill -KILL -u $USER"); }

int JXWM::GetClientTag(Client* c) 
{
    for (int i = 0; i < Clients.size(); i++)
    {
        for (auto client : Clients[i])
        {
            if (client.window == c->window) { return i; }
        }
    }
    return -1;
}

void JXWM::FullscreenClient(Client* c, bool toggle)
{
    if ((c == nullptr) || (c->isFullScreen == toggle)) { return; }
    c->isFullScreen = toggle;
    if (c->isFullScreen)
    {
        XWindowAttributes xwa;
        XGetWindowAttributes(disp, c->window, &xwa);
        c->og.x = xwa.x;
        c->og.y = xwa.y;
        c->og.w = xwa.width;
        c->og.h = xwa.height;
        XSetWindowBorderWidth(disp, c->window, 0);
        XMoveResizeWindow(disp, c->window, screenArea.x, screenArea.y, screenArea.w, screenArea.h);
        XChangeProperty(disp, c->window, netAtoms[NET_WM_STATE], XA_ATOM, 32, PropModeReplace, (unsigned char*)&netAtoms[NET_WM_STATE_FULLSCREEN], 1);
        FocusClient(c);
    }
    else 
    {
        XSetWindowBorderWidth(disp, c->window, settings.borderWidth);
        XMoveResizeWindow(disp, c->window, c->og.x, c->og.y, c->og.w, c->og.h);
        XDeleteProperty(disp, c->window, netAtoms[NET_WM_STATE]);
    }
}

void JXWM::UpdateClientList()
{
    std::vector<Window> cList;
    for (auto tag: Clients)
    {
        for (auto client: tag) 
        {
            cList.push_back(client.window);
        }
    }
    Window* clientList = cList.data();
    XChangeProperty(disp, root, netAtoms[NET_CLIENT_LIST], XA_WINDOW, 32, PropModeReplace, (unsigned char *)clientList, cList.size());
}

void JXWM::OnPropertyNotify(const XEvent& e)
{
    XPropertyEvent xpe = e.xproperty;
    if (xpe.window == root) { return; } 
    Client* c = GetClientFromWindow(xpe.window);
    //std::string name = (!c) ? "Unknown Client" : c->title;
    //logger.Log(INFO, "Got property notification: " + std::string(XGetAtomName(disp, xpe.atom)) + " from client " + name);
}
