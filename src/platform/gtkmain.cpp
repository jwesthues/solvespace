//-----------------------------------------------------------------------------
// Our main() function, and GTK2/3-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside platform/... should be standard C++ and OpenGL.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <iostream>

#include <json-c/json_object.h>
#include <json-c/json_util.h>

#include <glibmm/main.h>
#include <glibmm/convert.h>
#include <giomm/file.h>
#include <gdkmm/cursor.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/fixed.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/radiobuttongroup.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/main.h>

#if HAVE_GTK3
#include <gtkmm/hvbox.h>
#else
#include <gtkmm/box.h>
#endif

#include <cairomm/xlib_surface.h>
#include <pangomm/fontdescription.h>
#include <gdk/gdkx.h>
#include <fontconfig/fontconfig.h>

#include <GL/glx.h>

#include "solvespace.h"
#include "config.h"
#include "gloffscreen.h"

#ifdef HAVE_SPACEWARE
#   include <spnav.h>
#   ifndef SI_APP_FIT_BUTTON
#       define SI_APP_FIT_BUTTON 31
#   endif
#endif

namespace SolveSpace {
/* Settings */

/* Why not just use GSettings? Two reasons. It doesn't allow to easily see
   whether the setting had the default value, and it requires to install
   a schema globally. */
static json_object *settings = NULL;

static std::string CnfPrepare() {
    // Refer to http://standards.freedesktop.org/basedir-spec/latest/

    std::string dir;
    if(getenv("XDG_CONFIG_HOME")) {
        dir = std::string(getenv("XDG_CONFIG_HOME")) + "/solvespace";
    } else if(getenv("HOME")) {
        dir = std::string(getenv("HOME")) + "/.config/solvespace";
    } else {
        dbp("neither XDG_CONFIG_HOME nor HOME are set");
        return "";
    }

    struct stat st;
    if(stat(dir.c_str(), &st)) {
        if(errno == ENOENT) {
            if(mkdir(dir.c_str(), 0777)) {
                dbp("cannot mkdir %s: %s", dir.c_str(), strerror(errno));
                return "";
            }
        } else {
            dbp("cannot stat %s: %s", dir.c_str(), strerror(errno));
            return "";
        }
    } else if(!S_ISDIR(st.st_mode)) {
        dbp("%s is not a directory", dir.c_str());
        return "";
    }

    return dir + "/settings.json";
}

static void CnfLoad() {
    std::string path = CnfPrepare();
    if(path.empty())
        return;

    if(settings)
        json_object_put(settings); // deallocate

    settings = json_object_from_file(path.c_str());
    if(!settings) {
        if(errno != ENOENT)
            dbp("cannot load settings: %s", strerror(errno));

        settings = json_object_new_object();
    }
}

static void CnfSave() {
    std::string path = CnfPrepare();
    if(path.empty())
        return;

    /* json-c <0.12 has the first argument non-const here */
    if(json_object_to_file_ext((char*) path.c_str(), settings, JSON_C_TO_STRING_PRETTY))
        dbp("cannot save settings: %s", strerror(errno));
}

void CnfFreezeInt(uint32_t val, const std::string &key) {
    struct json_object *jval = json_object_new_int(val);
    json_object_object_add(settings, key.c_str(), jval);
    CnfSave();
}

uint32_t CnfThawInt(uint32_t val, const std::string &key) {
    struct json_object *jval;
    if(json_object_object_get_ex(settings, key.c_str(), &jval))
        return json_object_get_int(jval);
    else return val;
}

void CnfFreezeFloat(float val, const std::string &key) {
    struct json_object *jval = json_object_new_double(val);
    json_object_object_add(settings, key.c_str(), jval);
    CnfSave();
}

float CnfThawFloat(float val, const std::string &key) {
    struct json_object *jval;
    if(json_object_object_get_ex(settings, key.c_str(), &jval))
        return json_object_get_double(jval);
    else return val;
}

void CnfFreezeString(const std::string &val, const std::string &key) {
    struct json_object *jval = json_object_new_string(val.c_str());
    json_object_object_add(settings, key.c_str(), jval);
    CnfSave();
}

std::string CnfThawString(const std::string &val, const std::string &key) {
    struct json_object *jval;
    if(json_object_object_get_ex(settings, key.c_str(), &jval))
        return json_object_get_string(jval);
    return val;
}

static void CnfFreezeWindowPos(Gtk::Window *win, const std::string &key) {
    int x, y, w, h;
    win->get_position(x, y);
    win->get_size(w, h);

    CnfFreezeInt(x, key + "_left");
    CnfFreezeInt(y, key + "_top");
    CnfFreezeInt(w, key + "_width");
    CnfFreezeInt(h, key + "_height");
}

static void CnfThawWindowPos(Gtk::Window *win, const std::string &key) {
    int x, y, w, h;
    win->get_position(x, y);
    win->get_size(w, h);

    x = CnfThawInt(x, key + "_left");
    y = CnfThawInt(y, key + "_top");
    w = CnfThawInt(w, key + "_width");
    h = CnfThawInt(h, key + "_height");

    win->move(x, y);
    win->resize(w, h);
}

/* Timers */

int64_t GetMilliseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000 * (uint64_t) ts.tv_sec + ts.tv_nsec / 1000000;
}

static bool TimerCallback() {
    SS.GW.TimerCallback();
    SS.TW.TimerCallback();
    return false;
}

void SetTimerFor(int milliseconds) {
    Glib::signal_timeout().connect(&TimerCallback, milliseconds);
}

static bool AutosaveTimerCallback() {
    SS.Autosave();
    return false;
}

void SetAutosaveTimerFor(int minutes) {
    Glib::signal_timeout().connect(&AutosaveTimerCallback, minutes * 60 * 1000);
}

static bool LaterCallback() {
    SS.DoLater();
    return false;
}

void ScheduleLater() {
    Glib::signal_idle().connect(&LaterCallback);
}

/* GL wrapper */

#define GL_CHECK() \
    do { \
        int err = (int)glGetError(); \
        if(err) dbp("%s:%d: glGetError() == 0x%X %s", \
                    __FILE__, __LINE__, err, gluErrorString(err)); \
    } while (0)

class GlWidget : public Gtk::DrawingArea {
public:
    GlWidget() : _offscreen(NULL) {
        _xdisplay = gdk_x11_get_default_xdisplay();

        int glxmajor, glxminor;
        ssassert(glXQueryVersion(_xdisplay, &glxmajor, &glxminor),
                 "Expected OpenGL to be available");

        ssassert(glxmajor > 1 || (glxmajor == 1 && glxminor >= 3),
                 "Expected GLX >= 1.3");

        static int fbconfig_attrs[] = {
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            None
        };
        int fbconfig_num = 0;
        GLXFBConfig *fbconfigs = glXChooseFBConfig(_xdisplay, DefaultScreen(_xdisplay),
                fbconfig_attrs, &fbconfig_num);
        ssassert(fbconfigs && fbconfig_num > 0,
                 "Expected an available framebuffer configuration");

        /* prefer FBConfigs with depth of 32;
            * Mesa software rasterizer explodes with a BadMatch without this;
            * without this, Intel on Mesa flickers horribly for some reason.
           this does not seem to affect other rasterizers (ie NVidia).

           see this Mesa bug:
           http://lists.freedesktop.org/archives/mesa-dev/2015-January/074693.html */
        GLXFBConfig fbconfig = fbconfigs[0];
        for(int i = 0; i < fbconfig_num; i++) {
            XVisualInfo *visual_info = glXGetVisualFromFBConfig(_xdisplay, fbconfigs[i]);
            /* some GL visuals, notably on Chromium GL, do not have an associated
               X visual; this is not an obstacle as we always render offscreen. */
            if(!visual_info) continue;
            int depth = visual_info->depth;
            XFree(visual_info);

            if(depth == 32) {
                fbconfig = fbconfigs[i];
                break;
            }
        }

        _glcontext = glXCreateNewContext(_xdisplay,
                fbconfig, GLX_RGBA_TYPE, 0, True);
        ssassert(_glcontext != NULL, "Cannot create an OpenGL context");

        XFree(fbconfigs);

        /* create a dummy X window to create a rendering context against.
           we could use a Pbuffer, but some implementations (Chromium GL)
           don't support these. we could use an existing window, but
           some implementations (Chromium GL... do you see a pattern?)
           do really strange things, i.e. draw a black rectangle on
           the very front of the desktop if you do this. */
        _xwindow = XCreateSimpleWindow(_xdisplay,
                XRootWindow(_xdisplay, gdk_x11_get_default_screen()),
                /*x*/ 0, /*y*/ 0, /*width*/ 1, /*height*/ 1,
                /*border_width*/ 0, /*border*/ 0, /*background*/ 0);
    }

    ~GlWidget() {
        glXMakeCurrent(_xdisplay, None, NULL);

        XDestroyWindow(_xdisplay, _xwindow);

        delete _offscreen;

        glXDestroyContext(_xdisplay, _glcontext);
    }

protected:
    /* Draw on a GLX framebuffer object, then read pixels out and draw them on
       the Cairo context. Slower, but you get to overlay nice widgets. */
#ifdef HAVE_GTK3
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override {
#else
    bool on_expose_event(GdkEventExpose *) override {
        const Cairo::RefPtr<Cairo::Context> &cr = get_window()->create_cairo_context();
#endif
        ssassert(glXMakeCurrent(_xdisplay, _xwindow, _glcontext),
                 "Cannot make OpenGL context current");

        if(!_offscreen)
            _offscreen = new GLOffscreen;

        Gdk::Rectangle allocation = get_allocation();
        ssassert(_offscreen->begin(allocation.get_width(), allocation.get_height()),
                 "Cannot allocate offscreen rendering buffer");

        on_gl_draw();
        glFlush();
        GL_CHECK();

        Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(
                _offscreen->end(), Cairo::FORMAT_RGB24,
                allocation.get_width(), allocation.get_height(), allocation.get_width() * 4);
        cr->set_source(surface, 0, 0);
        cr->paint();
        surface->finish();

        return true;
    }

    virtual void on_gl_draw() = 0;

private:
    Display *_xdisplay;
    GLXContext _glcontext;
    GLOffscreen *_offscreen;
    ::Window _xwindow;
};

/* Editor overlay */

class EditorOverlay : public Gtk::Fixed {
public:
    EditorOverlay(Gtk::Widget &underlay) : _underlay(underlay) {
        set_size_request(0, 0);

        add(_underlay);

        _entry.set_no_show_all(true);
        _entry.set_has_frame(false);
        add(_entry);

        _entry.signal_activate().
            connect(sigc::mem_fun(this, &EditorOverlay::on_activate));
    }

    void start_editing(int x, int y, int font_height, bool is_monospace, int minWidthChars,
                       const std::string &val) {
        Pango::FontDescription font_desc;
        font_desc.set_family(is_monospace ? "monospace" : "normal");
        font_desc.set_absolute_size(font_height * Pango::SCALE);

#ifdef HAVE_GTK3
        /* For some reason override_font doesn't take screen DPI into
           account on GTK3 when working with font descriptors specified
           in absolute sizes; modify_font does on GTK2. */
        Pango::FontDescription override_font_desc(font_desc);
        double dpi = get_screen()->get_resolution();
        override_font_desc.set_size(font_height * (int)(72.0 / dpi) * Pango::SCALE);
        _entry.override_font(override_font_desc);
#else
        _entry.modify_font(font_desc);
#endif

        /* y coordinate denotes baseline */
        Pango::FontMetrics font_metrics = get_pango_context()->get_metrics(font_desc);
        y -= font_metrics.get_ascent() / Pango::SCALE;

        Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(get_pango_context());
        layout->set_font_description(font_desc);
        layout->set_text(val + " "); /* avoid scrolling */
        int width = layout->get_logical_extents().get_width();

#ifdef HAVE_GTK3
        Gtk::Border border = _entry.get_style_context()->get_padding();
        move(_entry, x - border.get_left(), y - border.get_top());
        _entry.set_width_chars(minWidthChars);
        _entry.set_size_request(width / Pango::SCALE, -1);
#else
        /* We need _gtk_entry_effective_inner_border, but it's not
           in the public API, so emulate its logic. */
        Gtk::Border border = { 2, 2, 2, 2 }, *style_border;
        gtk_widget_style_get(GTK_WIDGET(_entry.gobj()), "inner-border",
                             &style_border, NULL);
        if(style_border) border = *style_border;
        move(_entry, x - border.left, y - border.top);
        /* This is what set_width_chars does. */
        int minWidth = minWidthChars * std::max(font_metrics.get_approximate_digit_width(),
                                                font_metrics.get_approximate_char_width());
        _entry.set_size_request(std::max(width, minWidth) / Pango::SCALE, -1);
#endif

        _entry.set_text(val);
        if(!_entry.is_visible()) {
            _entry.show();
            _entry.grab_focus();
            _entry.add_modal_grab();
        }
    }

    void stop_editing() {
        if(_entry.is_visible())
            _entry.remove_modal_grab();
        _entry.hide();
    }

    bool is_editing() const {
        return _entry.is_visible();
    }

    sigc::signal<void, Glib::ustring> signal_editing_done() {
        return _signal_editing_done;
    }

    Gtk::Entry &get_entry() {
        return _entry;
    }

protected:
    bool on_key_press_event(GdkEventKey *event) override {
        if(event->keyval == GDK_KEY_Escape) {
            stop_editing();
            return true;
        }

        return false;
    }

    void on_size_allocate(Gtk::Allocation& allocation) override {
        Gtk::Fixed::on_size_allocate(allocation);

        _underlay.size_allocate(allocation);
    }

    void on_activate() {
        _signal_editing_done(_entry.get_text());
    }

private:
    Gtk::Widget &_underlay;
    Gtk::Entry _entry;
    sigc::signal<void, Glib::ustring> _signal_editing_done;
};

/* Graphics window */

double DeltaYOfScrollEvent(GdkEventScroll *event) {
#ifdef HAVE_GTK3
    double delta_y = event->delta_y;
#else
    double delta_y = 0;
#endif
    if(delta_y == 0) {
        switch(event->direction) {
            case GDK_SCROLL_UP:
            delta_y = -1;
            break;

            case GDK_SCROLL_DOWN:
            delta_y = 1;
            break;

            default:
            /* do nothing */
            return false;
        }
    }

    return delta_y;
}

class GraphicsWidget : public GlWidget {
public:
    GraphicsWidget() {
        set_events(Gdk::POINTER_MOTION_MASK |
                   Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON_MOTION_MASK |
                   Gdk::SCROLL_MASK |
                   Gdk::LEAVE_NOTIFY_MASK);
        set_double_buffered(true);
    }

protected:
    bool on_configure_event(GdkEventConfigure *event) override {
        _w = event->width;
        _h = event->height;

        return GlWidget::on_configure_event(event);;
    }

    void on_gl_draw() override {
        SS.GW.Paint();
    }

    bool on_motion_notify_event(GdkEventMotion *event) override {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        SS.GW.MouseMoved(x, y,
            event->state & GDK_BUTTON1_MASK,
            event->state & GDK_BUTTON2_MASK,
            event->state & GDK_BUTTON3_MASK,
            event->state & GDK_SHIFT_MASK,
            event->state & GDK_CONTROL_MASK);

        return true;
    }

    bool on_button_press_event(GdkEventButton *event) override {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        switch(event->button) {
            case 1:
            if(event->type == GDK_BUTTON_PRESS)
                SS.GW.MouseLeftDown(x, y);
            else if(event->type == GDK_2BUTTON_PRESS)
                SS.GW.MouseLeftDoubleClick(x, y);
            break;

            case 2:
            case 3:
            SS.GW.MouseMiddleOrRightDown(x, y);
            break;
        }

        return true;
    }

    bool on_button_release_event(GdkEventButton *event) override {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        switch(event->button) {
            case 1:
            SS.GW.MouseLeftUp(x, y);
            break;

            case 3:
            SS.GW.MouseRightUp(x, y);
            break;
        }

        return true;
    }

    bool on_scroll_event(GdkEventScroll *event) override {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        SS.GW.MouseScroll(x, y, (int)-DeltaYOfScrollEvent(event));

        return true;
    }

    bool on_leave_notify_event (GdkEventCrossing *) override {
        SS.GW.MouseLeave();

        return true;
    }

private:
    int _w, _h;
    void ij_to_xy(double i, double j, int &x, int &y) {
        // Convert to xy (vs. ij) style coordinates,
        // with (0, 0) at center
        x = (int)i - _w / 2;
        y = _h / 2 - (int)j;
    }
};

class GraphicsWindowGtk : public Gtk::Window {
public:
    GraphicsWindowGtk() : _overlay(_widget), _is_fullscreen(false) {
        set_default_size(900, 600);

        _box.pack_start(_menubar, false, true);
        _box.pack_start(_overlay, true, true);

        add(_box);

        _overlay.signal_editing_done().
            connect(sigc::mem_fun(this, &GraphicsWindowGtk::on_editing_done));
    }

    GraphicsWidget &get_widget() {
        return _widget;
    }

    EditorOverlay &get_overlay() {
        return _overlay;
    }

    Gtk::MenuBar &get_menubar() {
        return _menubar;
    }

    bool is_fullscreen() const {
        return _is_fullscreen;
    }

    bool emulate_key_press(GdkEventKey *event) {
        return on_key_press_event(event);
    }

protected:
    void on_show() override {
        Gtk::Window::on_show();

        CnfThawWindowPos(this, "GraphicsWindow");
    }

    void on_hide() override {
        CnfFreezeWindowPos(this, "GraphicsWindow");

        Gtk::Window::on_hide();
    }

    bool on_delete_event(GdkEventAny *) override {
        if(!SS.OkayToStartNewFile()) return true;
        SS.Exit();

        return true;
    }

    bool on_window_state_event(GdkEventWindowState *event) override {
        _is_fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

        /* The event arrives too late for the caller of ToggleFullScreen
           to notice state change; and it's possible that the WM will
           refuse our request, so we can't just toggle the saved state */
        SS.GW.EnsureValidActives();

        return Gtk::Window::on_window_state_event(event);
    }

    bool on_key_press_event(GdkEventKey *event) override {
        int chr;

        switch(event->keyval) {
            case GDK_KEY_Escape:
            chr = GraphicsWindow::ESCAPE_KEY;
            break;

            case GDK_KEY_Delete:
            chr = GraphicsWindow::DELETE_KEY;
            break;

            case GDK_KEY_Tab:
            chr = '\t';
            break;

            case GDK_KEY_BackSpace:
            case GDK_KEY_Back:
            chr = '\b';
            break;

            default:
            if(event->keyval >= GDK_KEY_F1 && event->keyval <= GDK_KEY_F12) {
                chr = GraphicsWindow::FUNCTION_KEY_BASE + (event->keyval - GDK_KEY_F1);
            } else {
                chr = gdk_keyval_to_unicode(event->keyval);
            }
        }

        if(event->state & GDK_SHIFT_MASK){
            chr |= GraphicsWindow::SHIFT_MASK;
        }
        if(event->state & GDK_CONTROL_MASK) {
            chr |= GraphicsWindow::CTRL_MASK;
        }

        if(chr && SS.GW.KeyDown(chr)) {
            return true;
        }

        if(chr == '\t') {
            // Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=123994.
            GraphicsWindow::MenuView(Command::SHOW_TEXT_WND);
            return true;
        }

        return Gtk::Window::on_key_press_event(event);
    }

    void on_editing_done(Glib::ustring value) {
        SS.GW.EditControlDone(value.c_str());
    }

private:
    GraphicsWidget _widget;
    EditorOverlay _overlay;
    Gtk::MenuBar _menubar;
    Gtk::VBox _box;

    bool _is_fullscreen;
};

std::unique_ptr<GraphicsWindowGtk> GW;

void GetGraphicsWindowSize(int *w, int *h) {
    Gdk::Rectangle allocation = GW->get_widget().get_allocation();
    *w = allocation.get_width();
    *h = allocation.get_height();
}

void InvalidateGraphics(void) {
    GW->get_widget().queue_draw();
}

void PaintGraphics(void) {
    GW->get_widget().queue_draw();
    /* Process animation */
    Glib::MainContext::get_default()->iteration(false);
}

void SetCurrentFilename(const std::string &filename) {
    if(!filename.empty()) {
        GW->set_title("SolveSpace - " + filename);
    } else {
        GW->set_title("SolveSpace - (not yet saved)");
    }
}

void ToggleFullScreen(void) {
    if(GW->is_fullscreen())
        GW->unfullscreen();
    else
        GW->fullscreen();
}

bool FullScreenIsActive(void) {
    return GW->is_fullscreen();
}

void ShowGraphicsEditControl(int x, int y, int fontHeight, int minWidthChars,
                             const std::string &val) {
    Gdk::Rectangle rect = GW->get_widget().get_allocation();

    // Convert to ij (vs. xy) style coordinates,
    // and compensate for the input widget height due to inverse coord
    int i, j;
    i = x + rect.get_width() / 2;
    j = -y + rect.get_height() / 2;

    GW->get_overlay().start_editing(i, j, fontHeight, /*is_monospace=*/false, minWidthChars, val);
}

void HideGraphicsEditControl(void) {
    GW->get_overlay().stop_editing();
}

bool GraphicsEditControlIsVisible(void) {
    return GW->get_overlay().is_editing();
}

/* TODO: removing menubar breaks accelerators. */
void ToggleMenuBar(void) {
    GW->get_menubar().set_visible(!GW->get_menubar().is_visible());
}

bool MenuBarIsVisible(void) {
    return GW->get_menubar().is_visible();
}

/* Context menus */

class ContextMenuItem : public Gtk::MenuItem {
public:
    static ContextCommand choice;

    ContextMenuItem(const Glib::ustring &label, ContextCommand cmd, bool mnemonic=false) :
            Gtk::MenuItem(label, mnemonic), _cmd(cmd) {
    }

protected:
    void on_activate() override {
        Gtk::MenuItem::on_activate();

        if(has_submenu())
            return;

        choice = _cmd;
    }

    /* Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=695488.
       This is used in addition to on_activate() to catch mouse events.
       Without on_activate(), it would be impossible to select a menu item
       via keyboard.
       This selects the item twice in some cases, but we are idempotent.
     */
    bool on_button_press_event(GdkEventButton *event) override {
        if(event->button == 1 && event->type == GDK_BUTTON_PRESS) {
            on_activate();
            return true;
        }

        return Gtk::MenuItem::on_button_press_event(event);
    }

private:
    ContextCommand _cmd;
};

ContextCommand ContextMenuItem::choice = ContextCommand::CANCELLED;

static Gtk::Menu *context_menu = NULL, *context_submenu = NULL;

void AddContextMenuItem(const char *label, ContextCommand cmd) {
    Gtk::MenuItem *menu_item;
    if(label)
        menu_item = new ContextMenuItem(label, cmd);
    else
        menu_item = new Gtk::SeparatorMenuItem();

    if(cmd == ContextCommand::SUBMENU) {
        menu_item->set_submenu(*context_submenu);
        context_submenu = NULL;
    }

    if(context_submenu) {
        context_submenu->append(*menu_item);
    } else {
        if(!context_menu)
            context_menu = new Gtk::Menu;

        context_menu->append(*menu_item);
    }
}

void CreateContextSubmenu(void) {
    ssassert(!context_submenu, "Unexpected nested submenu");

    context_submenu = new Gtk::Menu;
}

ContextCommand ShowContextMenu(void) {
    if(!context_menu)
        return ContextCommand::CANCELLED;

    Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
    context_menu->signal_deactivate().
        connect(sigc::mem_fun(loop.operator->(), &Glib::MainLoop::quit));

    ContextMenuItem::choice = ContextCommand::CANCELLED;

    context_menu->show_all();
    context_menu->popup(3, GDK_CURRENT_TIME);

    loop->run();

    delete context_menu;
    context_menu = NULL;

    return ContextMenuItem::choice;
}

/* Main menu */

template<class MenuItem> class MainMenuItem : public MenuItem {
public:
    MainMenuItem(const GraphicsWindow::MenuEntry &entry) :
            MenuItem(), _entry(entry), _synthetic(false) {
        Glib::ustring label(_entry.label);
        for(size_t i = 0; i < label.length(); i++) {
            if(label[i] == '&')
                label.replace(i, 1, "_");
        }

        guint accel_key = 0;
        Gdk::ModifierType accel_mods = Gdk::ModifierType();
        switch(_entry.accel) {
            case GraphicsWindow::DELETE_KEY:
            accel_key = GDK_KEY_Delete;
            break;

            case GraphicsWindow::ESCAPE_KEY:
            accel_key = GDK_KEY_Escape;
            break;

            case '\t':
            accel_key = GDK_KEY_Tab;
            break;

            default:
            accel_key = _entry.accel & ~(GraphicsWindow::SHIFT_MASK | GraphicsWindow::CTRL_MASK);
            if(accel_key > GraphicsWindow::FUNCTION_KEY_BASE &&
                    accel_key <= GraphicsWindow::FUNCTION_KEY_BASE + 12)
                accel_key = GDK_KEY_F1 + (accel_key - GraphicsWindow::FUNCTION_KEY_BASE - 1);
            else
                accel_key = gdk_unicode_to_keyval(accel_key);

            if(_entry.accel & GraphicsWindow::SHIFT_MASK)
                accel_mods |= Gdk::SHIFT_MASK;
            if(_entry.accel & GraphicsWindow::CTRL_MASK)
                accel_mods |= Gdk::CONTROL_MASK;
        }

        MenuItem::set_label(label);
        MenuItem::set_use_underline(true);
        if(!(accel_key & 0x01000000))
            MenuItem::set_accel_key(Gtk::AccelKey(accel_key, accel_mods));
    }

    void set_active(bool checked) {
        if(MenuItem::get_active() == checked)
            return;

       _synthetic = true;
        MenuItem::set_active(checked);
    }

protected:
    void on_activate() override {
        MenuItem::on_activate();

        if(_synthetic)
            _synthetic = false;
        else if(!MenuItem::has_submenu() && _entry.fn)
            _entry.fn(_entry.id);
    }

private:
    const GraphicsWindow::MenuEntry &_entry;
    bool _synthetic;
};

static std::map<uint32_t, Gtk::MenuItem *> main_menu_items;

static void InitMainMenu(Gtk::MenuShell *menu_shell) {
    Gtk::MenuItem *menu_item = NULL;
    Gtk::MenuShell *levels[5] = {menu_shell, 0};

    const GraphicsWindow::MenuEntry *entry = &GraphicsWindow::menu[0];
    int current_level = 0;
    while(entry->level >= 0) {
        if(entry->level > current_level) {
            Gtk::Menu *menu = new Gtk::Menu;
            menu_item->set_submenu(*menu);

            ssassert((unsigned)entry->level < sizeof(levels) / sizeof(levels[0]),
                     "Unexpected depth of menu nesting");

            levels[entry->level] = menu;
        }

        current_level = entry->level;

        if(entry->label) {
            switch(entry->kind) {
                case GraphicsWindow::MenuKind::NORMAL:
                menu_item = new MainMenuItem<Gtk::MenuItem>(*entry);
                break;

                case GraphicsWindow::MenuKind::CHECK:
                menu_item = new MainMenuItem<Gtk::CheckMenuItem>(*entry);
                break;

                case GraphicsWindow::MenuKind::RADIO:
                MainMenuItem<Gtk::CheckMenuItem> *radio_item =
                        new MainMenuItem<Gtk::CheckMenuItem>(*entry);
                radio_item->set_draw_as_radio(true);
                menu_item = radio_item;
                break;
            }
        } else {
            menu_item = new Gtk::SeparatorMenuItem();
        }

        levels[entry->level]->append(*menu_item);

        main_menu_items[(uint32_t)entry->id] = menu_item;

        ++entry;
    }
}

void EnableMenuByCmd(Command cmd, bool enabled) {
    main_menu_items[(uint32_t)cmd]->set_sensitive(enabled);
}

void CheckMenuByCmd(Command cmd, bool checked) {
    ((MainMenuItem<Gtk::CheckMenuItem>*)main_menu_items[(uint32_t)cmd])->set_active(checked);
}

void RadioMenuByCmd(Command cmd, bool selected) {
    SolveSpace::CheckMenuByCmd(cmd, selected);
}

class RecentMenuItem : public Gtk::MenuItem {
public:
    RecentMenuItem(const Glib::ustring& label, uint32_t cmd) :
            MenuItem(label), _cmd(cmd) {
    }

protected:
    void on_activate() override {
        if(_cmd >= (uint32_t)Command::RECENT_OPEN &&
           _cmd < ((uint32_t)Command::RECENT_OPEN + MAX_RECENT)) {
            SolveSpaceUI::MenuFile((Command)_cmd);
        } else if(_cmd >= (uint32_t)Command::RECENT_LINK &&
                  _cmd < ((uint32_t)Command::RECENT_LINK + MAX_RECENT)) {
            Group::MenuGroup((Command)_cmd);
        }
    }

private:
    uint32_t _cmd;
};

static void RefreshRecentMenu(Command cmd, Command base) {
    Gtk::MenuItem *recent = static_cast<Gtk::MenuItem*>(main_menu_items[(uint32_t)cmd]);
    recent->unset_submenu();

    Gtk::Menu *menu = new Gtk::Menu;
    recent->set_submenu(*menu);

    if(std::string(RecentFile[0]).empty()) {
        Gtk::MenuItem *placeholder = new Gtk::MenuItem("(no recent files)");
        placeholder->set_sensitive(false);
        menu->append(*placeholder);
    } else {
        for(size_t i = 0; i < MAX_RECENT; i++) {
            if(std::string(RecentFile[i]).empty())
                break;

            RecentMenuItem *item = new RecentMenuItem(RecentFile[i], (uint32_t)base + i);
            menu->append(*item);
        }
    }

    menu->show_all();
}

void RefreshRecentMenus(void) {
    RefreshRecentMenu(Command::OPEN_RECENT, Command::RECENT_OPEN);
    RefreshRecentMenu(Command::GROUP_RECENT, Command::RECENT_LINK);
}

/* Save/load */

static std::string ConvertFilters(std::string active, const FileFilter ssFilters[],
                                  Gtk::FileChooser *chooser) {
    for(const FileFilter *ssFilter = ssFilters; ssFilter->name; ssFilter++) {
#ifdef HAVE_GTK3
        Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
#else
        Gtk::FileFilter *filter = new Gtk::FileFilter;
#endif
        filter->set_name(ssFilter->name);

        bool is_active = false;
        std::string desc = "";
        for(const char *const *ssPattern = ssFilter->patterns; *ssPattern; ssPattern++) {
            std::string pattern = "*." + std::string(*ssPattern);
            filter->add_pattern(pattern);
            if(active == "")
                active = pattern.substr(2);
            if("*." + active == pattern)
                is_active = true;
            if(desc == "")
                desc = pattern;
            else
                desc += ", " + pattern;
        }
        filter->set_name(filter->get_name() + " (" + desc + ")");

#ifdef HAVE_GTK3
        chooser->add_filter(filter);
        if(is_active)
            chooser->set_filter(filter);
#else
        chooser->add_filter(*filter);
        if(is_active)
            chooser->set_filter(*filter);
#endif
    }

    return active;
}

bool GetOpenFile(std::string *filename, const std::string &activeOrEmpty,
                 const FileFilter filters[]) {
    Gtk::FileChooserDialog chooser(*GW, "SolveSpace - Open File");
    chooser.set_filename(*filename);
    chooser.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    chooser.add_button("_Open", Gtk::RESPONSE_OK);
    chooser.set_current_folder(CnfThawString("", "FileChooserPath"));

    ConvertFilters(activeOrEmpty, filters, &chooser);

    if(chooser.run() == Gtk::RESPONSE_OK) {
        CnfFreezeString(chooser.get_current_folder(), "FileChooserPath");
        *filename = chooser.get_filename();
        return true;
    } else {
        return false;
    }
}

/* Glib::path_get_basename got /removed/ in 3.0?! Come on */
static std::string Basename(std::string filename) {
    int slash = filename.rfind('/');
    if(slash >= 0)
        return filename.substr(slash + 1, filename.length());
    return "";
}

static void ChooserFilterChanged(Gtk::FileChooserDialog *chooser)
{
    /* Extract the pattern from the filter. GtkFileFilter doesn't provide
       any way to list the patterns, so we extract it from the filter name.
       Gross. */
    std::string filter_name = chooser->get_filter()->get_name();
    int lparen = filter_name.rfind('(') + 1;
    int rdelim = filter_name.find(',', lparen);
    if(rdelim < 0)
        rdelim = filter_name.find(')', lparen);
    ssassert(lparen > 0 && rdelim > 0, "Expected to find a parenthesized extension");

    std::string extension = filter_name.substr(lparen, rdelim - lparen);
    if(extension == "*")
        return;

    if(extension.length() > 2 && extension.substr(0, 2) == "*.")
        extension = extension.substr(2, extension.length() - 2);

    std::string basename = Basename(chooser->get_filename());
    int dot = basename.rfind('.');
    if(dot >= 0) {
        basename.replace(dot + 1, basename.length() - dot - 1, extension);
        chooser->set_current_name(basename);
    } else {
        chooser->set_current_name(basename + "." + extension);
    }
}

bool GetSaveFile(std::string *filename, const std::string &activeOrEmpty,
                 const FileFilter filters[]) {
    Gtk::FileChooserDialog chooser(*GW, "SolveSpace - Save File",
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    chooser.set_do_overwrite_confirmation(true);
    chooser.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    chooser.add_button("_Save", Gtk::RESPONSE_OK);

    std::string active = ConvertFilters(activeOrEmpty, filters, &chooser);

    chooser.set_current_folder(CnfThawString("", "FileChooserPath"));
    chooser.set_current_name(std::string("untitled.") + active);

    /* Gtk's dialog doesn't change the extension when you change the filter,
       and makes it extremely hard to do so. Gtk is garbage. */
    chooser.property_filter().signal_changed().
       connect(sigc::bind(sigc::ptr_fun(&ChooserFilterChanged), &chooser));

    if(chooser.run() == Gtk::RESPONSE_OK) {
        CnfFreezeString(chooser.get_current_folder(), "FileChooserPath");
        *filename = chooser.get_filename();
        return true;
    } else {
        return false;
    }
}

DialogChoice SaveFileYesNoCancel(void) {
    Glib::ustring message =
        "The file has changed since it was last saved.\n"
        "Do you want to save the changes?";
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title("SolveSpace - Modified File");
    dialog.add_button("_Save", Gtk::RESPONSE_YES);
    dialog.add_button("Do_n't Save", Gtk::RESPONSE_NO);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return DIALOG_YES;

        case Gtk::RESPONSE_NO:
        return DIALOG_NO;

        case Gtk::RESPONSE_CANCEL:
        default:
        return DIALOG_CANCEL;
    }
}

DialogChoice LoadAutosaveYesNo(void) {
    Glib::ustring message =
        "An autosave file is availible for this project.\n"
        "Do you want to load the autosave file instead?";
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title("SolveSpace - Autosave Available");
    dialog.add_button("_Load autosave", Gtk::RESPONSE_YES);
    dialog.add_button("Do_n't Load", Gtk::RESPONSE_NO);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return DIALOG_YES;

        case Gtk::RESPONSE_NO:
        default:
        return DIALOG_NO;
    }
}

DialogChoice LocateImportedFileYesNoCancel(const std::string &filename,
                                           bool canCancel) {
    Glib::ustring message =
        "The linked file " + filename + " is not present.\n"
        "Do you want to locate it manually?\n"
        "If you select \"No\", any geometry that depends on "
        "the missing file will be removed.";
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title("SolveSpace - Missing File");
    dialog.add_button("_Yes", Gtk::RESPONSE_YES);
    dialog.add_button("_No", Gtk::RESPONSE_NO);
    if(canCancel)
        dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return DIALOG_YES;

        case Gtk::RESPONSE_NO:
        return DIALOG_NO;

        case Gtk::RESPONSE_CANCEL:
        default:
        return DIALOG_CANCEL;
    }
}

/* Text window */

class TextWidget : public GlWidget {
public:
#ifdef HAVE_GTK3
    TextWidget(Glib::RefPtr<Gtk::Adjustment> adjustment) : _adjustment(adjustment) {
#else
    TextWidget(Gtk::Adjustment* adjustment) : _adjustment(adjustment) {
#endif
        set_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK |
                   Gdk::LEAVE_NOTIFY_MASK);
    }

    void set_cursor_hand(bool is_hand) {
        Glib::RefPtr<Gdk::Window> gdkwin = get_window();
        if(gdkwin) { // returns NULL if not realized
            Gdk::CursorType type = is_hand ? Gdk::HAND1 : Gdk::ARROW;
#ifdef HAVE_GTK3
            gdkwin->set_cursor(Gdk::Cursor::create(type));
#else
            gdkwin->set_cursor(Gdk::Cursor(type));
#endif
        }
    }

protected:
    void on_gl_draw() override {
        SS.TW.Paint();
    }

    bool on_motion_notify_event(GdkEventMotion *event) override {
        SS.TW.MouseEvent(/*leftClick*/ false,
                         /*leftDown*/ event->state & GDK_BUTTON1_MASK,
                         event->x, event->y);

        return true;
    }

    bool on_button_press_event(GdkEventButton *event) override {
        SS.TW.MouseEvent(/*leftClick*/ event->type == GDK_BUTTON_PRESS,
                         /*leftDown*/ event->state & GDK_BUTTON1_MASK,
                         event->x, event->y);

        return true;
    }

    bool on_scroll_event(GdkEventScroll *event) override {
        _adjustment->set_value(_adjustment->get_value() +
                DeltaYOfScrollEvent(event) * _adjustment->get_page_increment());

        return true;
    }

    bool on_leave_notify_event (GdkEventCrossing *) override {
        SS.TW.MouseLeave();

        return true;
    }

private:
#ifdef HAVE_GTK3
    Glib::RefPtr<Gtk::Adjustment> _adjustment;
#else
    Gtk::Adjustment *_adjustment;
#endif
};

class TextWindowGtk : public Gtk::Window {
public:
    TextWindowGtk() : _scrollbar(), _widget(_scrollbar.get_adjustment()),
                      _overlay(_widget), _box() {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
        set_skip_taskbar_hint(true);
        set_skip_pager_hint(true);
        set_title("SolveSpace - Property Browser");
        set_default_size(420, 300);

        _box.pack_start(_overlay, true, true);
        _box.pack_start(_scrollbar, false, true);
        add(_box);

        _scrollbar.get_adjustment()->signal_value_changed().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_scrollbar_value_changed));

        _overlay.signal_editing_done().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_editing_done));

        _overlay.get_entry().signal_motion_notify_event().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_editor_motion_notify_event));
        _overlay.get_entry().signal_button_press_event().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_editor_button_press_event));
    }

    Gtk::VScrollbar &get_scrollbar() {
        return _scrollbar;
    }

    TextWidget &get_widget() {
        return _widget;
    }

    EditorOverlay &get_overlay() {
        return _overlay;
    }

protected:
    void on_show() override {
        Gtk::Window::on_show();

        CnfThawWindowPos(this, "TextWindow");
    }

    void on_hide() override {
        CnfFreezeWindowPos(this, "TextWindow");

        Gtk::Window::on_hide();
    }

    bool on_key_press_event(GdkEventKey *event) override {
        if(GW->emulate_key_press(event)) {
            return true;
        }

        return Gtk::Window::on_key_press_event(event);
    }

    bool on_delete_event(GdkEventAny *) override {
        /* trigger the action and ignore the request */
        GraphicsWindow::MenuView(Command::SHOW_TEXT_WND);

        return false;
    }

    void on_scrollbar_value_changed() {
        SS.TW.ScrollbarEvent((int)_scrollbar.get_adjustment()->get_value());
    }

    void on_editing_done(Glib::ustring value) {
        SS.TW.EditControlDone(value.c_str());
    }

    bool on_editor_motion_notify_event(GdkEventMotion *event) {
        return _widget.event((GdkEvent*) event);
    }

    bool on_editor_button_press_event(GdkEventButton *event) {
        return _widget.event((GdkEvent*) event);
    }

private:
    Gtk::VScrollbar _scrollbar;
    TextWidget _widget;
    EditorOverlay _overlay;
    Gtk::HBox _box;
};

std::unique_ptr<TextWindowGtk> TW;

void ShowTextWindow(bool visible) {
    if(visible)
        TW->show();
    else
        TW->hide();
}

void GetTextWindowSize(int *w, int *h) {
    Gdk::Rectangle allocation = TW->get_widget().get_allocation();
    *w = allocation.get_width();
    *h = allocation.get_height();
}

void InvalidateText(void) {
    TW->get_widget().queue_draw();
}

void MoveTextScrollbarTo(int pos, int maxPos, int page) {
    TW->get_scrollbar().get_adjustment()->configure(pos, 0, maxPos, 1, 10, page);
}

void SetMousePointerToHand(bool is_hand) {
    TW->get_widget().set_cursor_hand(is_hand);
}

void ShowTextEditControl(int x, int y, const std::string &val) {
    TW->get_overlay().start_editing(x, y, TextWindow::CHAR_HEIGHT, /*is_monospace=*/true, 30, val);
}

void HideTextEditControl(void) {
    TW->get_overlay().stop_editing();
}

bool TextEditControlIsVisible(void) {
    return TW->get_overlay().is_editing();
}

/* Miscellanea */


void DoMessageBox(const char *message, int rows, int cols, bool error) {
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true,
                              error ? Gtk::MESSAGE_ERROR : Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK,
                              /*is_modal*/ true);
    dialog.set_title(error ? "SolveSpace - Error" : "SolveSpace - Message");
    dialog.run();
}

void OpenWebsite(const char *url) {
    gtk_show_uri(Gdk::Screen::get_default()->gobj(), url, GDK_CURRENT_TIME, NULL);
}

/* fontconfig is already initialized by GTK */
std::vector<std::string> GetFontFiles() {
    std::vector<std::string> fonts;

    FcPattern   *pat = FcPatternCreate();
    FcObjectSet *os  = FcObjectSetBuild(FC_FILE, (char *)0);
    FcFontSet   *fs  = FcFontList(0, pat, os);

    for(int i = 0; i < fs->nfont; i++) {
        FcChar8 *filenameFC = FcPatternFormat(fs->fonts[i], (const FcChar8*) "%{file}");
        std::string filename = (char*) filenameFC;
        fonts.push_back(filename);
        FcStrFree(filenameFC);
    }

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);

    return fonts;
}

static std::string resource_dir;
const void *LoadResource(const std::string &name, size_t *size) {
    static std::map<std::string, std::vector<uint8_t>> cache;

    auto it = cache.find(name);
    if(it == cache.end()) {
        struct stat st;
        std::string path;

        path = (UNIX_DATADIR "/") + name;
        if(stat(path.c_str(), &st)) {
            ssassert(errno == ENOENT, "Unexpected stat() error");
            ssassert(!resource_dir.empty(), "Expected local resource directory to be set");
            path = resource_dir + "/" + name;
            ssassert(!stat(path.c_str(), &st), "Cannot find resource");
        }

        std::vector<uint8_t> data(st.st_size);
        FILE *f = ssfopen(path.c_str(), "rb");
        ssassert(f != NULL, "Cannot open resource");
        fread(&data[0], 1, st.st_size, f);
        fclose(f);

        cache.emplace(name, std::move(data));
        it = cache.find(name);
    }

    *size = (*it).second.size();
    return &(*it).second[0];
}

/* Space Navigator support */

#ifdef HAVE_SPACEWARE
static GdkFilterReturn GdkSpnavFilter(GdkXEvent *gxevent, GdkEvent *, gpointer) {
    XEvent *xevent = (XEvent*) gxevent;

    spnav_event sev;
    if(!spnav_x11_event(xevent, &sev))
        return GDK_FILTER_CONTINUE;

    switch(sev.type) {
        case SPNAV_EVENT_MOTION:
            SS.GW.SpaceNavigatorMoved(
                (double)sev.motion.x,
                (double)sev.motion.y,
                (double)sev.motion.z  * -1.0,
                (double)sev.motion.rx *  0.001,
                (double)sev.motion.ry *  0.001,
                (double)sev.motion.rz * -0.001,
                xevent->xmotion.state & ShiftMask);
            break;

        case SPNAV_EVENT_BUTTON:
            if(!sev.button.press && sev.button.bnum == SI_APP_FIT_BUTTON) {
                SS.GW.SpaceNavigatorButtonUp();
            }
            break;
    }

    return GDK_FILTER_REMOVE;
}
#endif

/* Application lifecycle */

void ExitNow(void) {
    GW->hide();
    TW->hide();
}
};

int main(int argc, char** argv) {
    /* It would in principle be possible to judiciously use
       Glib::filename_{from,to}_utf8, but it's not really worth
       the effort.
       The setlocale() call is necessary for Glib::get_charset()
       to detect the system character set; otherwise it thinks
       it is always ANSI_X3.4-1968.
       We set it back to C after all.  */
    setlocale(LC_ALL, "");
    if(!Glib::get_charset()) {
        std::cerr << "Sorry, only UTF-8 locales are supported." << std::endl;
        return 1;
    }
    setlocale(LC_ALL, "C");

    /* If we don't do this, gtk_init will set the C standard library
       locale, and printf will format floats using ",". We will then
       fail to parse these. Also, many text window lines will become
       ambiguous. */
    gtk_disable_setlocale();

    /* Are we running from a build directory, as opposed to a global install? */
    if(std::string(argv[0]).find('/') != std::string::npos) {
        resource_dir = argv[0]; // .../src/solvespace
        resource_dir.erase(resource_dir.rfind('/')+1);
        resource_dir += "../res"; // .../res
    }

    Gtk::Main main(argc, argv);

#ifdef HAVE_SPACEWARE
    gdk_window_add_filter(NULL, GdkSpnavFilter, NULL);
#endif

    CnfLoad();

    SolveSpace::Pixmap icon = LoadPNG("freedesktop/solvespace-48x48.png");
    Glib::RefPtr<Gdk::Pixbuf> icon_gdk =
        Gdk::Pixbuf::create_from_data(&icon.data[0], Gdk::COLORSPACE_RGB,
                                      icon.hasAlpha, 8, icon.width, icon.height, icon.stride);

    TW.reset(new TextWindowGtk);
    GW.reset(new GraphicsWindowGtk);
    InitMainMenu(&GW->get_menubar());
    GW->get_menubar().accelerate(*TW);
    TW->set_transient_for(*GW);
    GW->set_icon(icon_gdk);
    TW->set_icon(icon_gdk);

    TW->show_all();
    GW->show_all();

#ifdef HAVE_SPACEWARE
#ifdef HAVE_GTK3
    // We don't care if it can't be opened; just continue without.
    spnav_x11_open(gdk_x11_get_default_xdisplay(),
                   gdk_x11_window_get_xid(GW->get_window()->gobj()));
#else
    spnav_x11_open(gdk_x11_get_default_xdisplay(),
                   GDK_WINDOW_XWINDOW(GW->get_window()->gobj()));
#endif
#endif

    SS.Init();

    if(argc >= 2) {
        if(argc > 2) {
            std::cerr << "Only the first file passed on command line will be opened."
                      << std::endl;
        }

        /* Make sure the argument is valid UTF-8. */
        SS.OpenFile(Glib::ustring(argv[1]));
    }

    main.run(*GW);

    TW.reset();
    GW.reset();

    SK.Clear();
    SS.Clear();

    return 0;
}
