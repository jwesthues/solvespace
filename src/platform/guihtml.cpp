//-----------------------------------------------------------------------------
// The Emscripten-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include <emscripten.h>
#include <emscripten/val.h>
#include <emscripten/html5.h>
#include "config.h"
#include "solvespace.h"

using namespace emscripten;

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Emscripten API bridging
//-----------------------------------------------------------------------------

#define sscheck(expr) do {                                                    \
        EMSCRIPTEN_RESULT emResult = (EMSCRIPTEN_RESULT)(expr);               \
        if(emResult < 0)                                                      \
            HandleError(__FILE__, __LINE__, __func__, #expr, emResult);       \
    } while(0)

static void HandleError(const char *file, int line, const char *function, const char *expr,
                 EMSCRIPTEN_RESULT emResult) {
    const char *error = "Unknown error";
    switch(emResult) {
        case EMSCRIPTEN_RESULT_DEFERRED:            error = "Deferred";               break;
        case EMSCRIPTEN_RESULT_NOT_SUPPORTED:       error = "Not supported";          break;
        case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED: error = "Failed (not deferred)";  break;
        case EMSCRIPTEN_RESULT_INVALID_TARGET:      error = "Invalid target";         break;
        case EMSCRIPTEN_RESULT_UNKNOWN_TARGET:      error = "Unknown target";         break;
        case EMSCRIPTEN_RESULT_INVALID_PARAM:       error = "Invalid parameter";      break;
        case EMSCRIPTEN_RESULT_FAILED:              error = "Failed";                 break;
        case EMSCRIPTEN_RESULT_NO_DATA:             error = "No data";                break;
    }

    std::string message;
    message += ssprintf("File %s, line %u, function %s:\n", file, line, function);
    message += ssprintf("Emscripten API call failed: %s.\n", expr);
    message += ssprintf("Error: %s\n", error);
    FatalError(message);
}

static val Wrap(std::string str) {
    // FIXME(emscripten): a nicer way to do this?
    EM_ASM($Wrap$ret = UTF8ToString($0), str.c_str());
    return val::global("window")["$Wrap$ret"];
}

static std::string Unwrap(val emStr) {
    // FIXME(emscripten): a nicer way to do this?
    val emArray = val::global("window").call<val>("intArrayFromString", emStr, true) ;
    val::global("window").set("$Wrap$input", emArray);
    char *strC = (char *)EM_ASM_INT(return allocate($Wrap$input, 'i8', ALLOC_NORMAL));
    std::string str(strC, emArray["length"].as<int>());
    free(strC);
    return str;
}

static void CallStdFunction(void *data) {
    std::function<void()> *func = (std::function<void()> *)data;
    if(*func) {
        (*func)();
    }
}

static val Wrap(std::function<void()> *func) {
    EM_ASM($Wrap$ret = Module.dynCall_vi.bind(null, $0, $1), CallStdFunction, func);
    return val::global("window")["$Wrap$ret"];
}

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    fprintf(stderr, "%s", message.c_str());
#ifndef NDEBUG
    emscripten_debugger();
#endif
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplHtml : public Settings {
public:
    void FreezeInt(const std::string &key, uint32_t value) {
        val::global("localStorage").call<void>("setItem", Wrap(key), value);
    }

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) {
        val value = val::global("localStorage").call<val>("getItem", Wrap(key));
        if(value == val::null())
            return defaultValue;
        return val::global("parseInt")(value, 0).as<int>();
    }

    void FreezeFloat(const std::string &key, double value) {
        val::global("localStorage").call<void>("setItem", Wrap(key), value);
    }

    double ThawFloat(const std::string &key, double defaultValue = 0.0) {
        val value = val::global("localStorage").call<val>("getItem", Wrap(key));
        if(value == val::null())
            return defaultValue;
        return val::global("parseFloat")(value).as<int>();
    }

    void FreezeString(const std::string &key, const std::string &value) {
        val::global("localStorage").call<void>("setItem", Wrap(key), value);
    }

    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") {
        val value = val::global("localStorage").call<val>("getItem", Wrap(key));
        if(value == val::null())
            return defaultValue;
        return Unwrap(value);
    }
};

SettingsRef GetSettings() {
    return std::make_shared<SettingsImplHtml>();
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplHtml : public Timer {
public:
    static void Callback(void *arg) {
        TimerImplHtml *timer = (TimerImplHtml *)arg;
        if(timer->onTimeout) {
            timer->onTimeout();
        }
    }

    void RunAfter(unsigned milliseconds) override {
        emscripten_async_call(TimerImplHtml::Callback, this, milliseconds + 1);
    }

    void RunAfterNextFrame() override {
        emscripten_async_call(TimerImplHtml::Callback, this, 0);
    }

    void RunAfterProcessingEvents() override {
        emscripten_push_uncounted_main_loop_blocker(TimerImplHtml::Callback, this);
    }
};

TimerRef CreateTimer() {
    return std::unique_ptr<TimerImplHtml>(new TimerImplHtml);
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuItemImplHtml : public MenuItem {
public:
    val htmlMenuItem;

    MenuItemImplHtml() :
        htmlMenuItem(val::global("document").call<val>("createElement", val("li")))
    {}

    void SetAccelerator(KeyboardEvent accel) override {
        val htmlAccel = htmlMenuItem.call<val>("querySelector", val(".accel"));
        if(htmlAccel.as<bool>()) {
            htmlAccel.call<void>("remove");
        }
        htmlAccel = val::global("document").call<val>("createElement", val("span"));
        htmlAccel.call<void>("setAttribute", val("class"), val("accel"));
        htmlAccel.set("innerText", AcceleratorDescription(accel));
        htmlMenuItem.call<void>("appendChild", htmlAccel);
    }

    void SetIndicator(Indicator type) override {
        val htmlClasses = htmlMenuItem["classList"];
        htmlClasses.call<void>("remove", val("check"));
        htmlClasses.call<void>("remove", val("radio"));
        switch(type) {
            case Indicator::NONE:
                break;

            case Indicator::CHECK_MARK:
                htmlClasses.call<void>("add", val("check"));
                break;

            case Indicator::RADIO_MARK:
                htmlClasses.call<void>("add", val("radio"));
                break;
        }
    }

    void SetEnabled(bool enabled) override {
        if(enabled) {
            htmlMenuItem["classList"].call<void>("remove", val("disabled"));
        } else {
            htmlMenuItem["classList"].call<void>("add", val("disabled"));
        }
    }

    void SetActive(bool active) override {
        if(active) {
            htmlMenuItem["classList"].call<void>("add", val("active"));
        } else {
            htmlMenuItem["classList"].call<void>("remove", val("active"));
        }
    }
};

class MenuImplHtml;

static std::shared_ptr<MenuImplHtml> popupMenuOnScreen;

class MenuImplHtml : public Menu,
                     public std::enable_shared_from_this<MenuImplHtml> {
public:
    val htmlMenu;

    std::vector<std::shared_ptr<MenuItemImplHtml>> menuItems;
    std::vector<std::shared_ptr<MenuImplHtml>>     subMenus;

    std::function<void()> popupDismissFunc;

    MenuImplHtml() :
        htmlMenu(val::global("document").call<val>("createElement", val("ul")))
    {
        htmlMenu["classList"].call<void>("add", val("menu"));
    }

    MenuItemRef AddItem(const std::string &label, std::function<void()> onTrigger,
                        bool mnemonics = true) override {
        std::shared_ptr<MenuItemImplHtml> menuItem = std::make_shared<MenuItemImplHtml>();
        menuItems.push_back(menuItem);
        menuItem->onTrigger = onTrigger;

        if(mnemonics) {
            val::global("window").call<void>("setLabelWithMnemonic", menuItem->htmlMenuItem,
                                             Wrap(label));
        } else {
            val htmlLabel = val::global("document").call<val>("createElement", val("span"));
            htmlLabel["classList"].call<void>("add", val("label"));
            htmlLabel["innerText"] = Wrap(label);
            menuItem->htmlMenuItem.call<void>("appendChild", htmlLabel);
        }
        menuItem->htmlMenuItem.call<void>("addEventListener", val("trigger"),
                                          Wrap(&menuItem->onTrigger));
        htmlMenu.call<void>("appendChild", menuItem->htmlMenuItem);
        return menuItem;
    }

    std::shared_ptr<Menu> AddSubMenu(const std::string &label) override {
        val htmlMenuItem = val::global("document").call<val>("createElement", val("li"));
        val::global("window").call<void>("setLabelWithMnemonic", htmlMenuItem, Wrap(label));
        htmlMenuItem["classList"].call<void>("add", val("has-submenu"));
        htmlMenu.call<void>("appendChild", htmlMenuItem);

        std::shared_ptr<MenuImplHtml> subMenu = std::make_shared<MenuImplHtml>();
        subMenus.push_back(subMenu);
        htmlMenuItem.call<void>("appendChild", subMenu->htmlMenu);
        return subMenu;
    }

    void AddSeparator() override {
        val htmlSeparator = val::global("document").call<val>("createElement", val("li"));
        htmlSeparator["classList"].call<void>("add", val("separator"));
        htmlMenu.call<void>("appendChild", htmlSeparator);
    }

    void PopUp() override {
        if(popupMenuOnScreen) {
            popupMenuOnScreen->htmlMenu.call<void>("remove");
            popupMenuOnScreen = NULL;
        }

        EmscriptenMouseEvent emStatus = {};
        sscheck(emscripten_get_mouse_status(&emStatus));
        htmlMenu["classList"].call<void>("add", val("popup"));
        htmlMenu["style"].set("left", std::to_string(emStatus.clientX) + "px");
        htmlMenu["style"].set("top",  std::to_string(emStatus.clientY) + "px");

        val::global("document")["body"].call<void>("appendChild", htmlMenu);
        popupMenuOnScreen = shared_from_this();
    }

    void Clear() override {
        while(htmlMenu["childElementCount"].as<int>() > 0) {
            htmlMenu["firstChild"].call<void>("remove");
        }
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplHtml>();
}

class MenuBarImplHtml final : public MenuBar {
public:
    val htmlMenuBar;

    std::vector<std::shared_ptr<MenuImplHtml>> subMenus;

    MenuBarImplHtml() :
        htmlMenuBar(val::global("document").call<val>("createElement", val("ul")))
    {
        htmlMenuBar["classList"].call<void>("add", val("menu"));
        htmlMenuBar["classList"].call<void>("add", val("menubar"));
    }

    std::shared_ptr<Menu> AddSubMenu(const std::string &label) override {
        val htmlMenuItem = val::global("document").call<val>("createElement", val("li"));
        val::global("window").call<void>("setLabelWithMnemonic", htmlMenuItem, Wrap(label));
        htmlMenuBar.call<void>("appendChild", htmlMenuItem);

        std::shared_ptr<MenuImplHtml> subMenu = std::make_shared<MenuImplHtml>();
        subMenus.push_back(subMenu);
        htmlMenuItem.call<void>("appendChild", subMenu->htmlMenu);
        return subMenu;
    }

    void Clear() override {
        while(htmlMenuBar["childElementCount"].as<int>() > 0) {
            htmlMenuBar["firstChild"].call<void>("remove");
        }
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::make_shared<MenuBarImplHtml>();
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

static KeyboardEvent handledKeyboardEvent;

class WindowImplHtml final : public Window {
public:
    std::string emCanvasId;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emContext = 0;

    val htmlContainer;
    val htmlEditor;

    std::function<void()> editingDoneFunc;
    std::shared_ptr<MenuBarImplHtml> menuBar;

    WindowImplHtml(val htmlContainer, std::string emCanvasId) :
        emCanvasId(emCanvasId),
        htmlContainer(htmlContainer),
        htmlEditor(val::global("document").call<val>("createElement", val("input")))
    {
        htmlEditor["classList"].call<void>("add", val("editor"));
        htmlEditor["style"].set("display", "none");
        editingDoneFunc = [this] {
            if(onEditingDone) {
                onEditingDone(Unwrap(htmlEditor["value"]));
            }
        };
        htmlEditor.call<void>("addEventListener", val("trigger"), Wrap(&editingDoneFunc));
        htmlContainer.call<void>("appendChild", htmlEditor);

        sscheck(emscripten_set_resize_callback(
                    "#window", this, /*useCapture=*/false,
                    WindowImplHtml::ResizeCallback));
        sscheck(emscripten_set_resize_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::ResizeCallback));
        sscheck(emscripten_set_mousemove_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_mousedown_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_click_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_dblclick_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_mouseup_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_mouseleave_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::MouseCallback));
        sscheck(emscripten_set_wheel_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::WheelCallback));
        sscheck(emscripten_set_keydown_callback(
                    "#window", this, /*useCapture=*/false,
                    WindowImplHtml::KeyboardCallback));
        sscheck(emscripten_set_keyup_callback(
                    "#window", this, /*useCapture=*/false,
                    WindowImplHtml::KeyboardCallback));
        sscheck(emscripten_set_webglcontextlost_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::ContextLostCallback));
        sscheck(emscripten_set_webglcontextrestored_callback(
                    emCanvasId.c_str(), this, /*useCapture=*/false,
                    WindowImplHtml::ContextRestoredCallback));

        ResizeCanvasElement();
        SetupWebGLContext();
    }

    ~WindowImplHtml() {
        if(emContext != 0) {
            sscheck(emscripten_webgl_destroy_context(emContext));
        }
    }

    static EM_BOOL ResizeCallback(int emEventType, const EmscriptenUiEvent *emEvent, void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        window->Invalidate();
        return EM_TRUE;
    }

    static EM_BOOL MouseCallback(int emEventType, const EmscriptenMouseEvent *emEvent,
                                 void *data) {
        if(val::global("window").call<bool>("isModal")) return EM_FALSE;

        WindowImplHtml *window = (WindowImplHtml *)data;
        MouseEvent event = {};
        switch(emEventType) {
            case EMSCRIPTEN_EVENT_MOUSEMOVE:
                event.type = MouseEvent::Type::MOTION;
                break;
            case EMSCRIPTEN_EVENT_MOUSEDOWN:
                event.type = MouseEvent::Type::PRESS;
                break;
            case EMSCRIPTEN_EVENT_DBLCLICK:
                event.type = MouseEvent::Type::DBL_PRESS;
                break;
            case EMSCRIPTEN_EVENT_MOUSEUP:
                event.type = MouseEvent::Type::RELEASE;
                break;
            case EMSCRIPTEN_EVENT_MOUSELEAVE:
                event.type = MouseEvent::Type::LEAVE;
                break;
            default:
                return EM_FALSE;
        }
        switch(emEventType) {
            case EMSCRIPTEN_EVENT_MOUSEMOVE:
                if(emEvent->buttons & 1) {
                    event.button = MouseEvent::Button::LEFT;
                } else if(emEvent->buttons & 2) {
                    event.button = MouseEvent::Button::RIGHT;
                } else if(emEvent->buttons & 4) {
                    event.button = MouseEvent::Button::MIDDLE;
                }
                break;
            case EMSCRIPTEN_EVENT_MOUSEDOWN:
            case EMSCRIPTEN_EVENT_DBLCLICK:
            case EMSCRIPTEN_EVENT_MOUSEUP:
                switch(emEvent->button) {
                    case 0: event.button = MouseEvent::Button::LEFT;   break;
                    case 1: event.button = MouseEvent::Button::MIDDLE; break;
                    case 2: event.button = MouseEvent::Button::RIGHT;  break;
                }
                break;
            default:
                return EM_FALSE;
        }
        event.x           = emEvent->targetX;
        event.y           = emEvent->targetY;
        event.shiftDown   = emEvent->shiftKey || emEvent->altKey;
        event.controlDown = emEvent->ctrlKey;

        if(window->onMouseEvent) {
            return window->onMouseEvent(event);
        }
        return EM_FALSE;
    }

    static EM_BOOL WheelCallback(int emEventType, const EmscriptenWheelEvent *emEvent,
                                 void *data) {
        if(val::global("window").call<bool>("isModal")) return EM_FALSE;

        WindowImplHtml *window = (WindowImplHtml *)data;
        MouseEvent event = {};
        if(emEvent->deltaY != 0) {
            event.type = MouseEvent::Type::SCROLL_VERT;
            event.scrollDelta = -emEvent->deltaY * 0.1;
        } else {
            return EM_FALSE;
        }

        EmscriptenMouseEvent emStatus = {};
        sscheck(emscripten_get_mouse_status(&emStatus));
        event.x           = emStatus.targetX;
        event.y           = emStatus.targetY;
        event.shiftDown   = emStatus.shiftKey;
        event.controlDown = emStatus.ctrlKey;

        if(window->onMouseEvent) {
            return window->onMouseEvent(event);
        }
        return EM_FALSE;
    }

    static EM_BOOL KeyboardCallback(int emEventType, const EmscriptenKeyboardEvent *emEvent,
                                    void *data) {
        if(emEvent->altKey) return EM_FALSE;
        if(emEvent->repeat) return EM_FALSE;

        WindowImplHtml *window = (WindowImplHtml *)data;
        KeyboardEvent event = {};
        switch(emEventType) {
            case EMSCRIPTEN_EVENT_KEYDOWN:
                event.type = KeyboardEvent::Type::PRESS;
                break;

            case EMSCRIPTEN_EVENT_KEYUP:
                event.type = KeyboardEvent::Type::RELEASE;
                break;

            default:
                return EM_FALSE;
        }
        event.shiftDown   = emEvent->shiftKey;
        event.controlDown = emEvent->ctrlKey;

        std::string key = emEvent->key;
        if(key[0] == 'F' && isdigit(key[1])) {
            event.key = KeyboardEvent::Key::FUNCTION;
            event.num = std::stol(key.substr(1));
        } else {
            event.key = KeyboardEvent::Key::CHARACTER;

            auto utf8 = ReadUTF8(key);
            if(++utf8.begin() == utf8.end()) {
                event.chr = tolower(*utf8.begin());
            } else if(key == "Escape") {
                event.chr = '\e';
            } else if(key == "Tab") {
                event.chr = '\t';
            } else if(key == "Backspace") {
                event.chr = '\b';
            } else if(key == "Delete") {
                event.chr = '\x7f';
            } else {
                return EM_FALSE;
            }

            if(event.chr == '>' && event.shiftDown) {
                event.shiftDown = false;
            }
        }

        if(event.Equals(handledKeyboardEvent)) return EM_FALSE;
        if(val::global("window").call<bool>("isModal")) {
            handledKeyboardEvent = {};
            return EM_FALSE;
        }

        if(window->onKeyboardEvent) {
            if(window->onKeyboardEvent(event)) {
                handledKeyboardEvent = event;
                return EM_TRUE;
            }
        }
        return EM_FALSE;
    }

    void SetupWebGLContext() {
        EmscriptenWebGLContextAttributes emAttribs = {};
        emscripten_webgl_init_context_attributes(&emAttribs);
        emAttribs.alpha = false;
        emAttribs.failIfMajorPerformanceCaveat = true;

        sscheck(emContext = emscripten_webgl_create_context(emCanvasId.c_str(), &emAttribs));
        dbp("Canvas %s: got context %d", emCanvasId.c_str(), emContext);
    }

    static int ContextLostCallback(int eventType, const void *reserved, void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        dbp("Canvas %s: context lost", window->emCanvasId.c_str());
        window->emContext = 0;

        if(window->onContextLost) {
            window->onContextLost();
        }
        return EM_TRUE;
    }

    static int ContextRestoredCallback(int eventType, const void *reserved, void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        dbp("Canvas %s: context restored", window->emCanvasId.c_str());
        window->SetupWebGLContext();
        return EM_TRUE;
    }

    void ResizeCanvasElement() {
        double width, height;
        std::string htmlContainerId = htmlContainer["id"].as<std::string>();
        sscheck(emscripten_get_element_css_size(htmlContainerId.c_str(), &width, &height));
        width  *= emscripten_get_device_pixel_ratio();
        height *= emscripten_get_device_pixel_ratio();
        int curWidth, curHeight;
        sscheck(emscripten_get_canvas_element_size(emCanvasId.c_str(), &curWidth, &curHeight));
        if(curWidth != (int)width || curHeight != (int)curHeight) {
            dbp("Canvas %s: resizing to (%g,%g)", emCanvasId.c_str(), width, height);
            sscheck(emscripten_set_canvas_element_size(
                        emCanvasId.c_str(), (int)width, (int)height));
        }
    }

    static void RenderCallback(void *data) {
        WindowImplHtml *window = (WindowImplHtml *)data;
        if(window->emContext == 0) {
            dbp("Canvas %s: cannot render: no context", window->emCanvasId.c_str());
            return;
        }

        window->ResizeCanvasElement();
        sscheck(emscripten_webgl_make_context_current(window->emContext));
        if(window->onRender) {
            window->onRender();
        }
    }

    double GetPixelDensity() override {
        return 96.0 * emscripten_get_device_pixel_ratio();
    }

    int GetDevicePixelRatio() override {
        return (int)emscripten_get_device_pixel_ratio();
    }

    bool IsVisible() override {
        // FIXME(emscripten): implement
        return true;
    }

    void SetVisible(bool visible) override {
        // FIXME(emscripten): implement
    }

    void Focus() override {
        // Do nothing, we can't affect focus of browser windows.
    }

    bool IsFullScreen() override {
        EmscriptenFullscreenChangeEvent emEvent = {};
        sscheck(emscripten_get_fullscreen_status(&emEvent));
        return emEvent.isFullscreen;
    }

    void SetFullScreen(bool fullScreen) override {
        if(fullScreen) {
            EmscriptenFullscreenStrategy emStrategy = {};
            emStrategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
            emStrategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
            emStrategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
            sscheck(emscripten_request_fullscreen_strategy(
                        emCanvasId.c_str(), /*deferUntilInEventHandler=*/true, &emStrategy));
        } else {
            sscheck(emscripten_exit_fullscreen());
        }
    }

    void SetTitle(const std::string &title) override {
        // FIXME(emscripten): implement
    }

    void SetMenuBar(MenuBarRef menuBar) override {
        std::shared_ptr<MenuBarImplHtml> menuBarImpl =
            std::static_pointer_cast<MenuBarImplHtml>(menuBar);
        this->menuBar = menuBarImpl;

        val htmlBody = val::global("document")["body"];
        val htmlCurrentMenuBar = htmlBody.call<val>("querySelector", val(".menubar"));
        if(htmlCurrentMenuBar.as<bool>()) {
            htmlCurrentMenuBar.call<void>("remove");
        }
        htmlBody.call<void>("insertBefore", menuBarImpl->htmlMenuBar,
                                 htmlBody["firstChild"]);
        ResizeCanvasElement();
    }

    void GetContentSize(double *width, double *height) override {
        sscheck(emscripten_get_element_css_size(emCanvasId.c_str(), width, height));
    }

    void SetMinContentSize(double width, double height) override {
        // Do nothing, we can't affect sizing of browser windows.
    }

    void FreezePosition(SettingsRef settings, const std::string &key) override {
        // Do nothing, we can't position browser windows.
    }

    void ThawPosition(SettingsRef settings, const std::string &key) override {
        // Do nothing, we can't position browser windows.
    }

    void SetCursor(Cursor cursor) override {
        std::string htmlCursor;
        switch(cursor) {
            case Cursor::POINTER: htmlCursor = "default"; break;
            case Cursor::HAND:    htmlCursor = "pointer"; break;
        }
        htmlContainer["style"].set("cursor", htmlCursor);
    }

    void SetTooltip(const std::string &text, double x, double y,
                    double width, double height) override {
        val htmlCanvas =
            val::global("document").call<val>("getElementById", emCanvasId);
        htmlCanvas.set("title", Wrap(text));
    }

    bool IsEditorVisible() override {
        return htmlEditor["style"]["display"].as<std::string>() != "none";
    }

    void ShowEditor(double x, double y, double fontHeight, double minWidth,
                    bool isMonospace, const std::string &text) override {
        htmlEditor["style"].set("display", val(""));
        htmlEditor["style"].set("left", std::to_string(x - 4) + "px");
        htmlEditor["style"].set("top",  std::to_string(y - fontHeight - 2) + "px");
        htmlEditor["style"].set("fontSize", std::to_string(fontHeight) + "px");
        htmlEditor["style"].set("minWidth", std::to_string(minWidth) + "px");
        htmlEditor["style"].set("fontFamily", isMonospace ? "monospace" : "sans");
        htmlEditor.set("value", Wrap(text));
        htmlEditor.call<void>("focus");
    }

    void HideEditor() override {
        htmlEditor["style"].set("display", val("none"));
    }

    void SetScrollbarVisible(bool visible) override {
        // FIXME(emscripten): implement
    }

    double scrollbarPos = 0.0;

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        // FIXME(emscripten): implement
    }

    double GetScrollbarPosition() override {
        // FIXME(emscripten): implement
        return scrollbarPos;
    }

    void SetScrollbarPosition(double pos) override {
        // FIXME(emscripten): implement
        scrollbarPos = pos;
    }

    void Invalidate() override {
        emscripten_async_call(WindowImplHtml::RenderCallback, this, -1);
    }
};

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    static int windowNum;

    std::string htmlContainerId = std::string("container") + std::to_string(windowNum);
    val htmlContainer =
        val::global("document").call<val>("getElementById", htmlContainerId);
    std::string emCanvasId = std::string("canvas") + std::to_string(windowNum);

    windowNum++;
    return std::make_shared<WindowImplHtml>(htmlContainer, emCanvasId);
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

void Open3DConnexion() {}
void Close3DConnexion() {}
void Request3DConnexionEventsForWindow(WindowRef window) {}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplHtml;

static std::vector<std::shared_ptr<MessageDialogImplHtml>> dialogsOnScreen;

class MessageDialogImplHtml final : public MessageDialog,
                                    public std::enable_shared_from_this<MessageDialogImplHtml> {
public:
    val htmlModal;
    val htmlDialog;
    val htmlMessage;
    val htmlDescription;
    val htmlButtons;

    std::vector<std::function<void()>> responseFuncs;

    MessageDialogImplHtml() :
        htmlModal(val::global("document").call<val>("createElement", val("div"))),
        htmlDialog(val::global("document").call<val>("createElement", val("div"))),
        htmlMessage(val::global("document").call<val>("createElement", val("strong"))),
        htmlDescription(val::global("document").call<val>("createElement", val("p"))),
        htmlButtons(val::global("document").call<val>("createElement", val("div")))
    {
        htmlModal["classList"].call<void>("add", val("modal"));
        htmlModal.call<void>("appendChild", htmlDialog);
        htmlDialog["classList"].call<void>("add", val("dialog"));
        htmlDialog.call<void>("appendChild", htmlMessage);
        htmlDialog.call<void>("appendChild", htmlDescription);
        htmlButtons["classList"].call<void>("add", val("buttons"));
        htmlDialog.call<void>("appendChild", htmlButtons);
    }

    void SetType(Type type) {
        // FIXME(emscripten): implement
    }

    void SetTitle(std::string title) {
        // FIXME(emscripten): implement
    }

    void SetMessage(std::string message) {
        htmlMessage.set("innerText", Wrap(message));
    }

    void SetDescription(std::string description) {
        htmlDescription.set("innerText", Wrap(description));
    }

    void AddButton(std::string label, Response response, bool isDefault = false) {
        val htmlButton = val::global("document").call<val>("createElement", val("div"));
        htmlButton["classList"].call<void>("add", val("button"));
        val::global("window").call<void>("setLabelWithMnemonic", htmlButton, Wrap(label));
        if(isDefault) {
            htmlButton["classList"].call<void>("add", val("default"), val("selected"));
        }

        std::function<void()> responseFunc = [this, response] {
            htmlModal.call<void>("remove");
            if(onResponse) {
                onResponse(response);
            }
            auto it = std::remove(dialogsOnScreen.begin(), dialogsOnScreen.end(),
                                  shared_from_this());
            dialogsOnScreen.erase(it);
        };
        responseFuncs.push_back(responseFunc);
        htmlButton.call<void>("addEventListener", val("trigger"), Wrap(&responseFuncs.back()));

        htmlButtons.call<void>("appendChild", htmlButton);
    }

    Response RunModal() {
        ssassert(false, "RunModal not supported on Emscripten");
    }

    void ShowModal() {
        dialogsOnScreen.push_back(shared_from_this());
        val::global("document")["body"].call<void>("appendChild", htmlModal);
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplHtml>();
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplHtml : public FileDialog {
public:
    // FIXME(emscripten): implement
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    // FIXME(emscripten): implement
    return std::shared_ptr<FileDialogImplHtml>();

}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    // FIXME(emscripten): implement
    return std::shared_ptr<FileDialogImplHtml>();
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles() {
    return {};
}

void OpenInBrowser(const std::string &url) {
    val::global("window").call<void>("open", Wrap(url));
}

std::vector<std::string> InitGui(int argc, char **argv) {
    static std::function<void()> onBeforeUnload = std::bind(&SolveSpaceUI::Exit, &SS);
    val::global("window").call<void>("addEventListener", val("beforeunload"),
                                     Wrap(&onBeforeUnload));

    // FIXME(emscripten): get locale from user preferences
    SetLocale("en_US");

    return {};
}

static void MainLoopIteration() {
    // We don't do anything here, as all our work is registered via timers.
}

void RunGui() {
    emscripten_set_main_loop(MainLoopIteration, 0, /*simulate_infinite_loop=*/true);
}

void ExitGui() {
    exit(0);
}

void ClearGui() {}

}
}
