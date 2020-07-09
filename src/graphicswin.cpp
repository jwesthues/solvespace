//-----------------------------------------------------------------------------
// Top-level implementation of the program's main window, in which a graphical
// representation of the model is drawn and edited by the user.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

typedef void MenuHandler(Command id);
using MenuKind = Platform::MenuItem::Indicator;
struct MenuEntry {
    int          level;          // 0 == on menu bar, 1 == one level down
    const char  *label;          // or NULL for a separator
    Command      cmd;            // command ID
    int          accel;          // keyboard accelerator
    MenuKind     kind;
    MenuHandler *fn;
};

#define mView (&GraphicsWindow::MenuView)
#define mEdit (&GraphicsWindow::MenuEdit)
#define mClip (&GraphicsWindow::MenuClipboard)
#define mReq  (&GraphicsWindow::MenuRequest)
#define mCon  (&Constraint::MenuConstrain)
#define mFile (&SolveSpaceUI::MenuFile)
#define mGrp  (&Group::MenuGroup)
#define mAna  (&SolveSpaceUI::MenuAnalyze)
#define mHelp (&SolveSpaceUI::MenuHelp)
#define SHIFT_MASK 0x100
#define CTRL_MASK  0x200
#define FN_MASK    0x400

#define S     SHIFT_MASK
#define C     CTRL_MASK
#define F     FN_MASK
#define KN    MenuKind::NONE
#define KC    MenuKind::CHECK_MARK
#define KR    MenuKind::RADIO_MARK
const MenuEntry Menu[] = {
//lv label                              cmd                        accel    kind
{ 0, N_("&File"),                       Command::NONE,             0,       KN, NULL   },
{ 1, N_("&New"),                        Command::NEW,              C|'n',   KN, mFile  },
{ 1, N_("&Open..."),                    Command::OPEN,             C|'o',   KN, mFile  },
{ 1, N_("Open &Recent"),                Command::OPEN_RECENT,      0,       KN, mFile  },
{ 1, N_("&Save"),                       Command::SAVE,             C|'s',   KN, mFile  },
{ 1, N_("Save &As..."),                 Command::SAVE_AS,          0,       KN, mFile  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Export &Image..."),            Command::EXPORT_IMAGE,     0,       KN, mFile  },
{ 1, N_("Export 2d &View..."),          Command::EXPORT_VIEW,      0,       KN, mFile  },
{ 1, N_("Export 2d &Section..."),       Command::EXPORT_SECTION,   0,       KN, mFile  },
{ 1, N_("Export 3d &Wireframe..."),     Command::EXPORT_WIREFRAME, 0,       KN, mFile  },
{ 1, N_("Export Triangle &Mesh..."),    Command::EXPORT_MESH,      0,       KN, mFile  },
{ 1, N_("Export &Surfaces..."),         Command::EXPORT_SURFACES,  0,       KN, mFile  },
{ 1, N_("Im&port..."),                  Command::IMPORT,           0,       KN, mFile  },
#ifndef __APPLE__
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("E&xit"),                       Command::EXIT,             C|'q',   KN, mFile  },
#endif

{ 0, N_("&Edit"),                       Command::NONE,             0,       KN, NULL   },
{ 1, N_("&Undo"),                       Command::UNDO,             C|'z',   KN, mEdit  },
{ 1, N_("&Redo"),                       Command::REDO,             C|'y',   KN, mEdit  },
{ 1, N_("Re&generate All"),             Command::REGEN_ALL,        ' ',     KN, mEdit  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Snap Selection to &Grid"),     Command::SNAP_TO_GRID,     '.',     KN, mEdit  },
{ 1, N_("Rotate Imported &90°"),        Command::ROTATE_90,        '9',     KN, mEdit  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Cu&t"),                        Command::CUT,              C|'x',   KN, mClip  },
{ 1, N_("&Copy"),                       Command::COPY,             C|'c',   KN, mClip  },
{ 1, N_("&Paste"),                      Command::PASTE,            C|'v',   KN, mClip  },
{ 1, N_("Paste &Transformed..."),       Command::PASTE_TRANSFORM,  C|'t',   KN, mClip  },
{ 1, N_("&Delete"),                     Command::DELETE,           '\x7f',  KN, mClip  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Select &Edge Chain"),          Command::SELECT_CHAIN,     C|'e',   KN, mEdit  },
{ 1, N_("Select &All"),                 Command::SELECT_ALL,       C|'a',   KN, mEdit  },
{ 1, N_("&Unselect All"),               Command::UNSELECT_ALL,     '\x1b',  KN, mEdit  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("&Line Styles..."),             Command::EDIT_LINE_STYLES, 0,       KN, mEdit  },
{ 1, N_("&View Projection..."),         Command::VIEW_PROJECTION,  0,       KN, mEdit  },
#ifndef __APPLE__
{ 1, N_("Con&figuration..."),           Command::CONFIGURATION,    0,       KN, mEdit  },
#endif

{ 0, N_("&View"),                       Command::NONE,             0,       KN, mView  },
{ 1, N_("Zoom &In"),                    Command::ZOOM_IN,          '+',     KN, mView  },
{ 1, N_("Zoom &Out"),                   Command::ZOOM_OUT,         '-',     KN, mView  },
{ 1, N_("Zoom To &Fit"),                Command::ZOOM_TO_FIT,      'f',     KN, mView  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Align View to &Workplane"),    Command::ONTO_WORKPLANE,   'w',     KN, mView  },
{ 1, N_("Nearest &Ortho View"),         Command::NEAREST_ORTHO,    F|2,     KN, mView  },
{ 1, N_("Nearest &Isometric View"),     Command::NEAREST_ISO,      F|3,     KN, mView  },
{ 1, N_("&Center View At Point"),       Command::CENTER_VIEW,      F|4,     KN, mView  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Show Snap &Grid"),             Command::SHOW_GRID,        '>',     KC, mView  },
{ 1, N_("Use &Perspective Projection"), Command::PERSPECTIVE_PROJ, '`',     KC, mView  },
{ 1, N_("Dimension &Units"),            Command::NONE,             0,       KN, NULL  },
{ 2, N_("Dimensions in &Millimeters"),  Command::UNITS_MM,         0,       KR, mView },
{ 2, N_("Dimensions in M&eters"),       Command::UNITS_METERS,     0,       KR, mView },
{ 2, N_("Dimensions in &Inches"),       Command::UNITS_INCHES,     0,       KR, mView },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("Show &Toolbar"),               Command::SHOW_TOOLBAR,     0,       KC, mView  },
{ 1, N_("Show Property Bro&wser"),      Command::SHOW_TEXT_WND,    '\t',    KC, mView  },
{ 1,  NULL,                             Command::NONE,             0,       KN, NULL   },
{ 1, N_("&Full Screen"),                Command::FULL_SCREEN,      C|F|11,  KC, mView  },

{ 0, N_("&New Group"),                  Command::NONE,             0,       KN, mGrp   },
{ 1, N_("Sketch In &3d"),               Command::GROUP_3D,         S|'3',   KN, mGrp   },
{ 1, N_("Sketch In New &Workplane"),    Command::GROUP_WRKPL,      S|'w',   KN, mGrp   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("Step &Translating"),           Command::GROUP_TRANS,      S|'t',   KN, mGrp   },
{ 1, N_("Step &Rotating"),              Command::GROUP_ROT,        S|'r',   KN, mGrp   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("E&xtrude"),                    Command::GROUP_EXTRUDE,    S|'x',   KN, mGrp   },
{ 1, N_("&Helix"),                      Command::GROUP_HELIX,      S|'h',   KN, mGrp   },
{ 1, N_("&Lathe"),                      Command::GROUP_LATHE,      S|'l',   KN, mGrp   },
{ 1, N_("Re&volve"),                    Command::GROUP_REVOLVE,    S|'v',   KN, mGrp   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("Link / Assemble..."),          Command::GROUP_LINK,       S|'i',   KN, mGrp   },
{ 1, N_("Link Recent"),                 Command::GROUP_RECENT,     0,       KN, mGrp   },

{ 0, N_("&Sketch"),                     Command::NONE,             0,       KN, mReq   },
{ 1, N_("In &Workplane"),               Command::SEL_WORKPLANE,    '2',     KR, mReq   },
{ 1, N_("Anywhere In &3d"),             Command::FREE_IN_3D,       '3',     KR, mReq   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("Datum &Point"),                Command::DATUM_POINT,      'p',     KN, mReq   },
{ 1, N_("&Workplane"),                  Command::WORKPLANE,        0,       KN, mReq   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("Line &Segment"),               Command::LINE_SEGMENT,     's',     KN, mReq   },
{ 1, N_("C&onstruction Line Segment"),  Command::CONSTR_SEGMENT,   S|'s',   KN, mReq   },
{ 1, N_("&Rectangle"),                  Command::RECTANGLE,        'r',     KN, mReq   },
{ 1, N_("&Circle"),                     Command::CIRCLE,           'c',     KN, mReq   },
{ 1, N_("&Arc of a Circle"),            Command::ARC,              'a',     KN, mReq   },
{ 1, N_("&Bezier Cubic Spline"),        Command::CUBIC,            'b',     KN, mReq   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("&Text in TrueType Font"),      Command::TTF_TEXT,         't',     KN, mReq   },
{ 1, N_("&Image"),                      Command::IMAGE,            0,       KN, mReq   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("To&ggle Construction"),        Command::CONSTRUCTION,     'g',     KN, mReq   },
{ 1, N_("Tangent &Arc at Point"),       Command::TANGENT_ARC,      S|'a',   KN, mReq   },
{ 1, N_("Split Curves at &Intersection"), Command::SPLIT_CURVES,   'i',     KN, mReq   },

{ 0, N_("&Constrain"),                  Command::NONE,             0,       KN, mCon   },
{ 1, N_("&Distance / Diameter"),        Command::DISTANCE_DIA,     'd',     KN, mCon   },
{ 1, N_("Re&ference Dimension"),        Command::REF_DISTANCE,     S|'d',   KN, mCon   },
{ 1, N_("A&ngle"),                      Command::ANGLE,            'n',     KN, mCon   },
{ 1, N_("Reference An&gle"),            Command::REF_ANGLE,        S|'n',   KN, mCon   },
{ 1, N_("Other S&upplementary Angle"),  Command::OTHER_ANGLE,      'u',     KN, mCon   },
{ 1, N_("Toggle R&eference Dim"),       Command::REFERENCE,        'e',     KN, mCon   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("&Horizontal"),                 Command::HORIZONTAL,       'h',     KN, mCon   },
{ 1, N_("&Vertical"),                   Command::VERTICAL,         'v',     KN, mCon   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("&On Point / Curve / Plane"),   Command::ON_ENTITY,        'o',     KN, mCon   },
{ 1, N_("E&qual Length / Radius / Angle"), Command::EQUAL,         'q',     KN, mCon   },
{ 1, N_("Length Ra&tio"),               Command::RATIO,            'z',     KN, mCon   },
{ 1, N_("Length Diff&erence"),          Command::DIFFERENCE,       'j',     KN, mCon   },
{ 1, N_("At &Midpoint"),                Command::AT_MIDPOINT,      'm',     KN, mCon   },
{ 1, N_("S&ymmetric"),                  Command::SYMMETRIC,        'y',     KN, mCon   },
{ 1, N_("Para&llel / Tangent"),         Command::PARALLEL,         'l',     KN, mCon   },
{ 1, N_("&Perpendicular"),              Command::PERPENDICULAR,    '[',     KN, mCon   },
{ 1, N_("Same Orient&ation"),           Command::ORIENTED_SAME,    'x',     KN, mCon   },
{ 1, N_("Lock Point Where &Dragged"),   Command::WHERE_DRAGGED,    ']',     KN, mCon   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("Comment"),                     Command::COMMENT,          ';',     KN, mCon   },

{ 0, N_("&Analyze"),                    Command::NONE,             0,       KN, mAna   },
{ 1, N_("Measure &Volume"),             Command::VOLUME,           C|S|'v', KN, mAna   },
{ 1, N_("Measure A&rea"),               Command::AREA,             C|S|'a', KN, mAna   },
{ 1, N_("Measure &Perimeter"),          Command::PERIMETER,        C|S|'p', KN, mAna   },
{ 1, N_("Show &Interfering Parts"),     Command::INTERFERENCE,     C|S|'i', KN, mAna   },
{ 1, N_("Show &Naked Edges"),           Command::NAKED_EDGES,      C|S|'n', KN, mAna   },
{ 1, N_("Show &Center of Mass"),        Command::CENTER_OF_MASS,   C|S|'c', KN, mAna   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("Show &Underconstrained Points"), Command::SHOW_DOF,       C|S|'f', KN, mAna   },
{ 1, NULL,                              Command::NONE,             0,       KN, NULL   },
{ 1, N_("&Trace Point"),                Command::TRACE_PT,         C|S|'t', KN, mAna   },
{ 1, N_("&Stop Tracing..."),            Command::STOP_TRACING,     C|S|'s', KN, mAna   },
{ 1, N_("Step &Dimension..."),          Command::STEP_DIM,         C|S|'d', KN, mAna   },

{ 0, N_("&Help"),                       Command::NONE,             0,       KN, mHelp  },
{ 1, N_("&Language"),                   Command::LOCALE,           0,       KN, mHelp  },
{ 1, N_("&Website / Manual"),           Command::WEBSITE,          0,       KN, mHelp  },
#ifndef __APPLE__
{ 1, N_("&About"),                      Command::ABOUT,            0,       KN, mHelp  },
#endif
{ -1, 0,                                Command::NONE,             0,       KN, NULL   }
};
#undef S
#undef C
#undef F
#undef KN
#undef KC
#undef KR

void GraphicsWindow::ActivateCommand(Command cmd) {
    for(int i = 0; Menu[i].level >= 0; i++) {
        if(cmd == Menu[i].cmd) {
            (Menu[i].fn)((Command)Menu[i].cmd);
            break;
        }
    }
}

Platform::KeyboardEvent GraphicsWindow::AcceleratorForCommand(Command cmd) {
    int rawAccel = 0;
    for(int i = 0; Menu[i].level >= 0; i++) {
        if(cmd == Menu[i].cmd) {
            rawAccel = Menu[i].accel;
            break;
        }
    }

    Platform::KeyboardEvent accel = {};
    accel.type = Platform::KeyboardEvent::Type::PRESS;
    if(rawAccel & SHIFT_MASK) {
        accel.shiftDown = true;
    }
    if(rawAccel & CTRL_MASK) {
        accel.controlDown = true;
    }
    if(rawAccel & FN_MASK) {
        accel.key = Platform::KeyboardEvent::Key::FUNCTION;
        accel.num = rawAccel & 0xff;
    } else {
        accel.key = Platform::KeyboardEvent::Key::CHARACTER;
        accel.chr = (char)(rawAccel & 0xff);
    }

    return accel;
}

bool GraphicsWindow::KeyboardEvent(Platform::KeyboardEvent event) {
    using Platform::KeyboardEvent;

    if(event.type == KeyboardEvent::Type::RELEASE)
        return true;

    if(event.key == KeyboardEvent::Key::CHARACTER) {
        if(event.chr == '\b') {
            // Treat backspace identically to escape.
            MenuEdit(Command::UNSELECT_ALL);
            return true;
        } else if(event.chr == '=') {
            // Treat = as +. This is specific to US (and US-compatible) keyboard layouts,
            // but makes zooming from keyboard much more usable on these.
            // Ideally we'd have a platform-independent way of binding to a particular
            // physical key regardless of shift status...
            MenuView(Command::ZOOM_IN);
            return true;
        }
    }

    // On some platforms, the OS does not handle some or all keyboard accelerators,
    // so handle them here.
    for(int i = 0; Menu[i].level >= 0; i++) {
        if(AcceleratorForCommand(Menu[i].cmd).Equals(event)) {
            ActivateCommand(Menu[i].cmd);
            return true;
        }
    }

    return false;
}

void GraphicsWindow::PopulateMainMenu() {
    bool unique = false;
    Platform::MenuBarRef mainMenu = Platform::GetOrCreateMainMenu(&unique);
    if(unique) mainMenu->Clear();

    Platform::MenuRef currentSubMenu;
    std::vector<Platform::MenuRef> subMenuStack;
    for(int i = 0; Menu[i].level >= 0; i++) {
        while(Menu[i].level > 0 && Menu[i].level <= (int)subMenuStack.size()) {
            currentSubMenu = subMenuStack.back();
            subMenuStack.pop_back();
        }

        if(Menu[i].label == NULL) {
            currentSubMenu->AddSeparator();
            continue;
        }

        std::string label = Translate(Menu[i].label);
        if(Menu[i].level == 0) {
            currentSubMenu = mainMenu->AddSubMenu(label);
        } else if(Menu[i].cmd == Command::OPEN_RECENT) {
            openRecentMenu = currentSubMenu->AddSubMenu(label);
        } else if(Menu[i].cmd == Command::GROUP_RECENT) {
            linkRecentMenu = currentSubMenu->AddSubMenu(label);
        } else if(Menu[i].cmd == Command::LOCALE) {
            Platform::MenuRef localeMenu = currentSubMenu->AddSubMenu(label);
            for(const Locale &locale : Locales()) {
                localeMenu->AddItem(locale.displayName, [&]() {
                    SetLocale(locale.Culture());
                    Platform::GetSettings()->FreezeString("Locale", locale.Culture());

                    SS.UpdateWindowTitles();
                    PopulateMainMenu();
                    EnsureValidActives();
                });
            }
        } else if(Menu[i].fn == NULL) {
            subMenuStack.push_back(currentSubMenu);
            currentSubMenu = currentSubMenu->AddSubMenu(label);
        } else {
            Platform::MenuItemRef menuItem = currentSubMenu->AddItem(label);
            menuItem->SetIndicator(Menu[i].kind);
            if(Menu[i].accel != 0) {
                menuItem->SetAccelerator(AcceleratorForCommand(Menu[i].cmd));
            }
            menuItem->onTrigger = std::bind(Menu[i].fn, Menu[i].cmd);

            if(Menu[i].cmd == Command::SHOW_GRID) {
                showGridMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::PERSPECTIVE_PROJ) {
                perspectiveProjMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::SHOW_TOOLBAR) {
                showToolbarMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::SHOW_TEXT_WND) {
                showTextWndMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::FULL_SCREEN) {
                fullScreenMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::UNITS_MM) {
                unitsMmMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::UNITS_METERS) {
                unitsMetersMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::UNITS_INCHES) {
                unitsInchesMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::SEL_WORKPLANE) {
                inWorkplaneMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::FREE_IN_3D) {
                in3dMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::UNDO) {
                undoMenuItem = menuItem;
            } else if(Menu[i].cmd == Command::REDO) {
                redoMenuItem = menuItem;
            }
        }
    }

    PopulateRecentFiles();
    SS.UndoEnableMenus();

    window->SetMenuBar(mainMenu);
}

static void PopulateMenuWithPathnames(Platform::MenuRef menu,
                                      std::vector<Platform::Path> pathnames,
                                      std::function<void(const Platform::Path &)> onTrigger) {
    menu->Clear();
    if(pathnames.empty()) {
        Platform::MenuItemRef menuItem = menu->AddItem(_("(no recent files)"));
        menuItem->SetEnabled(false);
    } else {
        for(Platform::Path pathname : pathnames) {
            Platform::MenuItemRef menuItem = menu->AddItem(pathname.raw, [=]() {
                if(FileExists(pathname)) {
                    onTrigger(pathname);
                } else {
                    Error(_("File '%s' does not exist."), pathname.raw.c_str());
                }
            }, /*mnemonics=*/false);
        }
    }
}

void GraphicsWindow::PopulateRecentFiles() {
    PopulateMenuWithPathnames(openRecentMenu, SS.recentFiles, [](const Platform::Path &path) {
        if(!SS.OkayToStartNewFile()) return;
        SS.Load(path);
    });

    PopulateMenuWithPathnames(linkRecentMenu, SS.recentFiles, [](const Platform::Path &path) {
        Group::MenuGroup(Command::GROUP_LINK, path);
    });
}

void GraphicsWindow::Init() {
    scale     = 5;
    offset    = Vector::From(0, 0, 0);
    projRight = Vector::From(1, 0, 0);
    projUp    = Vector::From(0, 1, 0);

    // Make sure those are valid; could get a mouse move without a mouse
    // down if someone depresses the button, then drags into our window.
    orig.projRight = projRight;
    orig.projUp = projUp;

    // And with the last group active
    ssassert(!SK.groupOrder.IsEmpty(),
             "Group order can't be empty since we will activate the last group.");
    activeGroup = *SK.groupOrder.Last();
    SK.GetGroup(activeGroup)->Activate();

    showWorkplanes = false;
    showNormals = true;
    showPoints = true;
    showConstruction = true;
    showConstraints = true;
    showShaded = true;
    showEdges = true;
    showMesh = false;
    showOutlines = false;
    drawOccludedAs = DrawOccludedAs::INVISIBLE;

    showTextWindow = true;

    showSnapGrid = false;
    context.active = false;
    toolbarHovered = Command::NONE;

    if(!window) {
        window = Platform::CreateWindow();
        if(window) {
            using namespace std::placeholders;
            // Do this first, so that if it causes an onRender event we don't try to paint without
            // a canvas.
            window->SetMinContentSize(720, 670);
            window->onClose = std::bind(&SolveSpaceUI::MenuFile, Command::EXIT);
            window->onContextLost = [&] {
                canvas = NULL;
                persistentCanvas = NULL;
                persistentDirty = true;
            };
            window->onRender = std::bind(&GraphicsWindow::Paint, this);
            window->onKeyboardEvent = std::bind(&GraphicsWindow::KeyboardEvent, this, _1);
            window->onMouseEvent = std::bind(&GraphicsWindow::MouseEvent, this, _1);
            window->onSixDofEvent = std::bind(&GraphicsWindow::SixDofEvent, this, _1);
            window->onEditingDone = std::bind(&GraphicsWindow::EditControlDone, this, _1);
            PopulateMainMenu();
        }
    }

    if(window) {
        canvas = CreateRenderer();
        if(canvas) {
            persistentCanvas = canvas->CreateBatch();
            persistentDirty = true;
        }
    }

    // Do this last, so that all the menus get updated correctly.
    ClearSuper();
}

void GraphicsWindow::AnimateOntoWorkplane() {
    if(!LockedInWorkplane()) return;

    Entity *w = SK.GetEntity(ActiveWorkplane());
    Quaternion quatf = w->Normal()->NormalGetNum();

    // Get Z pointing vertical, if we're on turntable nav mode:
    if(SS.turntableNav) {
        Vector normalRight = quatf.RotationU();
        Vector normalUp    = quatf.RotationV();
        Vector normal      = normalRight.Cross(normalUp);
        if(normalRight.z != 0) {
            double theta = atan2(normalUp.z, normalRight.z);
            theta -= atan2(1, 0);
            normalRight = normalRight.RotatedAbout(normal, theta);
            normalUp    = normalUp.RotatedAbout(normal, theta);
            quatf       = Quaternion::From(normalRight, normalUp);
        }
    }

    Vector offsetf = (SK.GetEntity(w->point[0])->PointGetNum()).ScaledBy(-1);

    // If the view screen is open, then we need to refresh it.
    SS.ScheduleShowTW();

    AnimateOnto(quatf, offsetf);
}

void GraphicsWindow::AnimateOnto(Quaternion quatf, Vector offsetf) {
    // Get our initial orientation and translation.
    Quaternion quat0 = Quaternion::From(projRight, projUp);
    Vector offset0 = offset;

    // Make sure we take the shorter of the two possible paths.
    double mp = (quatf.Minus(quat0)).Magnitude();
    double mm = (quatf.Plus(quat0)).Magnitude();
    if(mp > mm) {
        quatf = quatf.ScaledBy(-1);
        mp = mm;
    }
    double mo = (offset0.Minus(offsetf)).Magnitude()*scale;

    // Animate transition, unless it's a tiny move.
    int64_t t0 = GetMilliseconds();
    int32_t dt = (mp < 0.01 && mo < 10) ? (-20) :
                     (int32_t)(100 + 1000*mp + 0.4*mo);
    // Don't ever animate for longer than 2000 ms; we can get absurdly
    // long translations (as measured in pixels) if the user zooms out, moves,
    // and then zooms in again.
    if(dt > 2000) dt = 2000;
    Quaternion dq = quatf.Times(quat0.Inverse());

    if(!animateTimer) {
        animateTimer = Platform::CreateTimer();
    }
    animateTimer->onTimeout = [=] {
        int64_t tn = GetMilliseconds();
        if((tn - t0) < dt) {
            animateTimer->RunAfterNextFrame();

            double s = (tn - t0)/((double)dt);
            offset = (offset0.ScaledBy(1 - s)).Plus(offsetf.ScaledBy(s));
            Quaternion quat = (dq.ToThe(s)).Times(quat0).WithMagnitude(1);

            projRight = quat.RotationU();
            projUp    = quat.RotationV();
        } else {
            projRight = quatf.RotationU();
            projUp    = quatf.RotationV();
            offset    = offsetf;
        }
        window->Invalidate();
    };
    animateTimer->RunAfterNextFrame();
}

void GraphicsWindow::HandlePointForZoomToFit(Vector p, Point2d *pmax, Point2d *pmin,
                                             double *wmin, bool usePerspective,
                                             const Camera &camera)
{
    double w;
    Vector pp = camera.ProjectPoint4(p, &w);
    // If usePerspective is true, then we calculate a perspective projection of the point.
    // If not, then we do a parallel projection regardless of the current
    // scale factor.
    if(usePerspective) {
        pp = pp.ScaledBy(1.0/w);
    }

    pmax->x = max(pmax->x, pp.x);
    pmax->y = max(pmax->y, pp.y);
    pmin->x = min(pmin->x, pp.x);
    pmin->y = min(pmin->y, pp.y);
    *wmin = min(*wmin, w);
}
void GraphicsWindow::LoopOverPoints(const std::vector<Entity *> &entities,
                                    const std::vector<Constraint *> &constraints,
                                    const std::vector<hEntity> &faces,
                                    Point2d *pmax, Point2d *pmin, double *wmin,
                                    bool usePerspective, bool includeMesh,
                                    const Camera &camera) {

    for(Entity *e : entities) {
        if(e->IsPoint()) {
            HandlePointForZoomToFit(e->PointGetNum(), pmax, pmin, wmin, usePerspective, camera);
        } else if(e->type == Entity::Type::CIRCLE) {
            // Lots of entities can extend outside the bbox of their points,
            // but circles are particularly bad. We want to get things halfway
            // reasonable without the mesh, because a zoom to fit is used to
            // set the zoom level to set the chord tol.
            double r = e->CircleGetRadiusNum();
            Vector c = SK.GetEntity(e->point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(e->normal)->NormalGetNum();
            for(int j = 0; j < 4; j++) {
                Vector p = (j == 0) ? (c.Plus(q.RotationU().ScaledBy( r))) :
                           (j == 1) ? (c.Plus(q.RotationU().ScaledBy(-r))) :
                           (j == 2) ? (c.Plus(q.RotationV().ScaledBy( r))) :
                                      (c.Plus(q.RotationV().ScaledBy(-r)));
                HandlePointForZoomToFit(p, pmax, pmin, wmin, usePerspective, camera);
            }
        } else {
            // We have to iterate children points, because we can select entities without points
            for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
                if(e->point[i].v == 0) break;
                Vector p = SK.GetEntity(e->point[i])->PointGetNum();
                HandlePointForZoomToFit(p, pmax, pmin, wmin, usePerspective, camera);
            }
        }
    }

    for(Constraint *c : constraints) {
        std::vector<Vector> refs;
        c->GetReferencePoints(camera, &refs);
        for(Vector p : refs) {
            HandlePointForZoomToFit(p, pmax, pmin, wmin, usePerspective, camera);
        }
    }

    if(!includeMesh && faces.empty()) return;

    Group *g = SK.GetGroup(activeGroup);
    g->GenerateDisplayItems();
    for(int i = 0; i < g->displayMesh.l.n; i++) {
        STriangle *tr = &(g->displayMesh.l[i]);
        if(!includeMesh) {
            bool found = false;
            for(const hEntity &face : faces) {
                if(face.v != tr->meta.face) continue;
                found = true;
                break;
            }
            if(!found) continue;
        }
        HandlePointForZoomToFit(tr->a, pmax, pmin, wmin, usePerspective, camera);
        HandlePointForZoomToFit(tr->b, pmax, pmin, wmin, usePerspective, camera);
        HandlePointForZoomToFit(tr->c, pmax, pmin, wmin, usePerspective, camera);
    }
    if(!includeMesh) return;
    for(int i = 0; i < g->polyLoops.l.n; i++) {
        SContour *sc = &(g->polyLoops.l[i]);
        for(int j = 0; j < sc->l.n; j++) {
            HandlePointForZoomToFit(sc->l[j].p, pmax, pmin, wmin, usePerspective, camera);
        }
    }
}
void GraphicsWindow::ZoomToFit(bool includingInvisibles, bool useSelection) {
    if(!window) return;

    scale = ZoomToFit(GetCamera(), includingInvisibles, useSelection);
}
double GraphicsWindow::ZoomToFit(const Camera &camera,
                                 bool includingInvisibles, bool useSelection) {
    std::vector<Entity *> entities;
    std::vector<Constraint *> constraints;
    std::vector<hEntity> faces;

    if(useSelection) {
        for(int i = 0; i < selection.n; i++) {
            Selection *s = &selection[i];
            if(s->entity.v != 0) {
                Entity *e = SK.entity.FindById(s->entity);
                if(e->IsFace()) {
                    faces.push_back(e->h);
                    continue;
                }
                entities.push_back(e);
            }
            if(s->constraint.v != 0) {
                Constraint *c = SK.constraint.FindById(s->constraint);
                constraints.push_back(c);
            }
        }
    }

    bool selectionUsed = !entities.empty() || !constraints.empty() || !faces.empty();

    if(!selectionUsed) {
        for(Entity &e : SK.entity) {
            // we don't want to handle separate points, because we will iterate them inside entities.
            if(e.IsPoint()) continue;
            if(!includingInvisibles && !e.IsVisible()) continue;
            entities.push_back(&e);
        }

        for(Constraint &c : SK.constraint) {
            if(!c.IsVisible()) continue;
            constraints.push_back(&c);
        }
    }

    // On the first run, ignore perspective.
    Point2d pmax = { -1e12, -1e12 }, pmin = { 1e12, 1e12 };
    double wmin = 1;
    LoopOverPoints(entities, constraints, faces, &pmax, &pmin, &wmin,
                   /*usePerspective=*/false, /*includeMesh=*/!selectionUsed,
                   camera);

    double xm = (pmax.x + pmin.x)/2, ym = (pmax.y + pmin.y)/2;
    double dx = pmax.x - pmin.x, dy = pmax.y - pmin.y;

    offset = offset.Plus(projRight.ScaledBy(-xm)).Plus(
                         projUp.   ScaledBy(-ym));

    // And based on this, we calculate the scale and offset
    double scale;
    if(EXACT(dx == 0 && dy == 0)) {
        scale = 5;
    } else {
        double scalex = 1e12, scaley = 1e12;
        if(EXACT(dx != 0)) scalex = 0.9*camera.width /dx;
        if(EXACT(dy != 0)) scaley = 0.9*camera.height/dy;
        scale = min(scalex, scaley);

        scale = min(300.0, scale);
        scale = max(0.003, scale);
    }

    // Then do another run, considering the perspective.
    pmax.x = -1e12; pmax.y = -1e12;
    pmin.x =  1e12; pmin.y =  1e12;
    wmin = 1;
    LoopOverPoints(entities, constraints, faces, &pmax, &pmin, &wmin,
                   /*usePerspective=*/true, /*includeMesh=*/!selectionUsed,
                   camera);

    // Adjust the scale so that no points are behind the camera
    if(wmin < 0.1) {
        double k = camera.tangent;
        // w = 1+k*scale*z
        double zmin = (wmin - 1)/(k*scale);
        // 0.1 = 1 + k*scale*zmin
        // (0.1 - 1)/(k*zmin) = scale
        scale = min(scale, (0.1 - 1)/(k*zmin));
    }

    return scale;
}

void GraphicsWindow::MenuView(Command id) {
    switch(id) {
        case Command::ZOOM_IN:
            SS.GW.scale *= 1.2;
            SS.ScheduleShowTW();
            break;

        case Command::ZOOM_OUT:
            SS.GW.scale /= 1.2;
            SS.ScheduleShowTW();
            break;

        case Command::ZOOM_TO_FIT:
            SS.GW.ZoomToFit(/*includingInvisibles=*/false, /*useSelection=*/true);
            SS.ScheduleShowTW();
            break;

        case Command::SHOW_GRID:
            SS.GW.showSnapGrid = !SS.GW.showSnapGrid;
            SS.GW.EnsureValidActives();
            SS.GW.Invalidate();
            if(SS.GW.showSnapGrid && !SS.GW.LockedInWorkplane()) {
                Message(_("No workplane is active, so the grid will not appear."));
            }
            break;

        case Command::PERSPECTIVE_PROJ:
            SS.usePerspectiveProj = !SS.usePerspectiveProj;
            SS.GW.EnsureValidActives();
            SS.GW.Invalidate();
            if(SS.cameraTangent < 1e-6) {
                Error(_("The perspective factor is set to zero, so the view will "
                        "always be a parallel projection.\n\n"
                        "For a perspective projection, modify the perspective "
                        "factor in the configuration screen. A value around 0.3 "
                        "is typical."));
            }
            break;

        case Command::ONTO_WORKPLANE:
            if(SS.GW.LockedInWorkplane()) {
                SS.GW.AnimateOntoWorkplane();
                break;
            }  // if not in 2d mode use ORTHO logic
            // fallthrough
        case Command::NEAREST_ORTHO:
        case Command::NEAREST_ISO: {
            static const Vector ortho[3] = {
                Vector::From(1, 0, 0),
                Vector::From(0, 1, 0),
                Vector::From(0, 0, 1)
            };
            double sqrt2 = sqrt(2.0), sqrt6 = sqrt(6.0);
            Quaternion quat0 = Quaternion::From(SS.GW.projRight, SS.GW.projUp);
            Quaternion quatf = quat0;
            double dmin = 1e10;

            // There are 24 possible views; 3*2*2*2
            int i, j, negi, negj;
            for(i = 0; i < 3; i++) {
                for(j = 0; j < 3; j++) {
                    if(i == j) continue;
                    for(negi = 0; negi < 2; negi++) {
                        for(negj = 0; negj < 2; negj++) {
                            Vector ou = ortho[i], ov = ortho[j];
                            if(negi) ou = ou.ScaledBy(-1);
                            if(negj) ov = ov.ScaledBy(-1);
                            Vector on = ou.Cross(ov);

                            Vector u, v;
                            if(id == Command::NEAREST_ORTHO || id == Command::ONTO_WORKPLANE) {
                                u = ou;
                                v = ov;
                            } else {
                                u =
                                    ou.ScaledBy(1/sqrt2).Plus(
                                    on.ScaledBy(-1/sqrt2));
                                v =
                                    ou.ScaledBy(-1/sqrt6).Plus(
                                    ov.ScaledBy(2/sqrt6).Plus(
                                    on.ScaledBy(-1/sqrt6)));
                            }

                            Quaternion quatt = Quaternion::From(u, v);
                            double d = min(
                                (quatt.Minus(quat0)).Magnitude(),
                                (quatt.Plus(quat0)).Magnitude());
                            if(d < dmin) {
                                dmin = d;
                                quatf = quatt;
                            }
                        }
                    }
                }
            }

            SS.GW.AnimateOnto(quatf, SS.GW.offset);
            break;
        }

        case Command::CENTER_VIEW:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                Quaternion quat0;
                // Offset is the selected point, quaternion is same as before
                Vector pt = SK.GetEntity(SS.GW.gs.point[0])->PointGetNum();
                quat0 = Quaternion::From(SS.GW.projRight, SS.GW.projUp);
                SS.GW.ClearSelection();
                SS.GW.AnimateOnto(quat0, pt.ScaledBy(-1));
            } else {
                Error(_("Select a point; this point will become the center "
                        "of the view on screen."));
            }
            break;

        case Command::SHOW_TOOLBAR:
            SS.showToolbar = !SS.showToolbar;
            SS.GW.EnsureValidActives();
            SS.GW.Invalidate();
            break;

        case Command::SHOW_TEXT_WND:
            SS.GW.showTextWindow = !SS.GW.showTextWindow;
            SS.GW.EnsureValidActives();
            break;

        case Command::UNITS_INCHES:
            SS.viewUnits = Unit::INCHES;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case Command::UNITS_MM:
            SS.viewUnits = Unit::MM;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case Command::UNITS_METERS:
            SS.viewUnits = Unit::METERS;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case Command::FULL_SCREEN:
            SS.GW.window->SetFullScreen(!SS.GW.window->IsFullScreen());
            SS.GW.EnsureValidActives();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
    SS.GW.Invalidate();
}

void GraphicsWindow::EnsureValidActives() {
    bool change = false;
    // The active group must exist, and not be the references.
    Group *g = SK.group.FindByIdNoOops(activeGroup);
    if((!g) || (g->h == Group::HGROUP_REFERENCES)) {
        // Not using range-for because this is used to find an index.
        int i;
        for(i = 0; i < SK.groupOrder.n; i++) {
            if(SK.groupOrder[i] != Group::HGROUP_REFERENCES) {
                break;
            }
        }
        if(i >= SK.groupOrder.n) {
            // This can happen if the user deletes all the groups in the
            // sketch. It's difficult to prevent that, because the last
            // group might have been deleted automatically, because it failed
            // a dependency. There needs to be something, so create a plane
            // drawing group and activate that. They should never be able
            // to delete the references, though.
            activeGroup = SS.CreateDefaultDrawingGroup();
            // We've created the default group, but not the workplane entity;
            // do it now so that drawing mode isn't switched to "Free in 3d".
            SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        } else {
            activeGroup = SK.groupOrder[i];
        }
        SK.GetGroup(activeGroup)->Activate();
        change = true;
    }

    // The active coordinate system must also exist.
    if(LockedInWorkplane()) {
        Entity *e = SK.entity.FindByIdNoOops(ActiveWorkplane());
        if(e) {
            hGroup hgw = e->group;
            if(hgw != activeGroup && SS.GroupsInOrder(activeGroup, hgw)) {
                // The active workplane is in a group that comes after the
                // active group; so any request or constraint will fail.
                SetWorkplaneFreeIn3d();
                change = true;
            }
        } else {
            SetWorkplaneFreeIn3d();
            change = true;
        }
    }

    if(!window) return;

    // And update the checked state for various menus
    bool locked = LockedInWorkplane();
    in3dMenuItem->SetActive(!locked);
    inWorkplaneMenuItem->SetActive(locked);

    SS.UndoEnableMenus();

    switch(SS.viewUnits) {
        case Unit::MM:
        case Unit::METERS:
        case Unit::INCHES:
            break;
        default:
            SS.viewUnits = Unit::MM;
            break;
    }
    unitsMmMenuItem->SetActive(SS.viewUnits == Unit::MM);
    unitsMetersMenuItem->SetActive(SS.viewUnits == Unit::METERS);
    unitsInchesMenuItem->SetActive(SS.viewUnits == Unit::INCHES);

    if(SS.TW.window) SS.TW.window->SetVisible(SS.GW.showTextWindow);
    showTextWndMenuItem->SetActive(SS.GW.showTextWindow);

    showGridMenuItem->SetActive(SS.GW.showSnapGrid);
    perspectiveProjMenuItem->SetActive(SS.usePerspectiveProj);
    showToolbarMenuItem->SetActive(SS.showToolbar);
    fullScreenMenuItem->SetActive(SS.GW.window->IsFullScreen());

    if(change) SS.ScheduleShowTW();
}

void GraphicsWindow::SetWorkplaneFreeIn3d() {
    SK.GetGroup(activeGroup)->activeWorkplane = Entity::FREE_IN_3D;
}
hEntity GraphicsWindow::ActiveWorkplane() {
    Group *g = SK.group.FindByIdNoOops(activeGroup);
    if(g) {
        return g->activeWorkplane;
    } else {
        return Entity::FREE_IN_3D;
    }
}
bool GraphicsWindow::LockedInWorkplane() {
    return (SS.GW.ActiveWorkplane() != Entity::FREE_IN_3D);
}

void GraphicsWindow::ForceTextWindowShown() {
    if(!showTextWindow) {
        showTextWindow = true;
        showTextWndMenuItem->SetActive(true);
        SS.TW.window->SetVisible(true);
    }
}

void GraphicsWindow::DeleteTaggedRequests() {
    Request *r;
    // Delete any requests that were affected by this deletion.
    for(r = SK.request.First(); r; r = SK.request.NextAfter(r)) {
        if(r->workplane == Entity::FREE_IN_3D) continue;
        if(!r->workplane.isFromRequest()) continue;
        Request *wrkpl = SK.GetRequest(r->workplane.request());
        if(wrkpl->tag)
            r->tag = 1;
    }
    // Rewrite any point-coincident constraints that were affected by this
    // deletion.
    for(r = SK.request.First(); r; r = SK.request.NextAfter(r)) {
        if(!r->tag) continue;
        FixConstraintsForRequestBeingDeleted(r->h);
    }
    // and then delete the tagged requests.
    SK.request.RemoveTagged();

    // An edit might be in progress for the just-deleted item. So
    // now it's not.
    window->HideEditor();
    SS.TW.HideEditControl();
    // And clear out the selection, which could contain that item.
    ClearSuper();
    // And regenerate to get rid of what it generates, plus anything
    // that references it (since the regen code checks for that).
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    EnsureValidActives();
    SS.ScheduleShowTW();
}

Vector GraphicsWindow::SnapToGrid(Vector p) {
    if(!LockedInWorkplane()) return p;

    EntityBase *wrkpl = SK.GetEntity(ActiveWorkplane()),
               *norm  = wrkpl->Normal();
    Vector wo = SK.GetEntity(wrkpl->point[0])->PointGetNum(),
           wu = norm->NormalU(),
           wv = norm->NormalV(),
           wn = norm->NormalN();

    Vector pp = (p.Minus(wo)).DotInToCsys(wu, wv, wn);
    pp.x = floor((pp.x / SS.gridSpacing) + 0.5)*SS.gridSpacing;
    pp.y = floor((pp.y / SS.gridSpacing) + 0.5)*SS.gridSpacing;
    pp.z = 0;

    return pp.ScaleOutOfCsys(wu, wv, wn).Plus(wo);
}

void GraphicsWindow::MenuEdit(Command id) {
    switch(id) {
        case Command::UNSELECT_ALL:
            SS.GW.GroupSelection();
            // If there's nothing selected to de-select, and no operation
            // to cancel, then perhaps they want to return to the home
            // screen in the text window.
            if(SS.GW.gs.n               == 0 &&
               SS.GW.gs.constraints     == 0 &&
               SS.GW.pending.operation  == Pending::NONE)
            {
                if(!(SS.TW.window->IsEditorVisible() ||
                     SS.GW.window->IsEditorVisible()))
                {
                    if(SS.TW.shown.screen == TextWindow::Screen::STYLE_INFO) {
                        SS.TW.GoToScreen(TextWindow::Screen::LIST_OF_STYLES);
                    } else {
                        SS.TW.ClearSuper();
                    }
                }
            }
            SS.GW.ClearSuper();
            SS.TW.HideEditControl();
            SS.nakedEdges.Clear();
            SS.justExportedInfo.draw = false;
            SS.centerOfMass.draw = false;
            // This clears the marks drawn to indicate which points are
            // still free to drag.
            Param *p;
            for(p = SK.param.First(); p; p = SK.param.NextAfter(p)) {
                p->free = false;
            }
            if(SS.exportMode) {
                SS.exportMode = false;
                SS.GenerateAll(SolveSpaceUI::Generate::ALL);
            }
            SS.GW.persistentDirty = true;
            break;

        case Command::SELECT_ALL: {
            Entity *e;
            for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
                if(e->group != SS.GW.activeGroup) continue;
                if(e->IsFace() || e->IsDistance()) continue;
                if(!e->IsVisible()) continue;

                SS.GW.MakeSelected(e->h);
            }
            SS.GW.Invalidate();
            SS.ScheduleShowTW();
            break;
        }

        case Command::SELECT_CHAIN: {
            Entity *e;
            int newlySelected = 0;
            bool didSomething;
            do {
                didSomething = false;
                for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
                    if(e->group != SS.GW.activeGroup) continue;
                    if(!e->HasEndpoints()) continue;
                    if(!e->IsVisible()) continue;

                    Vector st = e->EndpointStart(),
                           fi = e->EndpointFinish();

                    bool onChain = false, alreadySelected = false;
                    List<Selection> *ls = &(SS.GW.selection);
                    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
                        if(!s->entity.v) continue;
                        if(s->entity == e->h) {
                            alreadySelected = true;
                            continue;
                        }
                        Entity *se = SK.GetEntity(s->entity);
                        if(!se->HasEndpoints()) continue;

                        Vector sst = se->EndpointStart(),
                               sfi = se->EndpointFinish();

                        if(sst.Equals(st) || sst.Equals(fi) ||
                           sfi.Equals(st) || sfi.Equals(fi))
                        {
                            onChain = true;
                        }
                    }
                    if(onChain && !alreadySelected) {
                        SS.GW.MakeSelected(e->h);
                        newlySelected++;
                        didSomething = true;
                    }
                }
            } while(didSomething);
            SS.GW.Invalidate();
            SS.ScheduleShowTW();
            if(newlySelected == 0) {
                Error(_("No additional entities share endpoints with the selected entities."));
            }
            break;
        }

        case Command::ROTATE_90: {
            SS.GW.GroupSelection();
            Entity *e = NULL;
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                e = SK.GetEntity(SS.GW.gs.point[0]);
            } else if(SS.GW.gs.n == 1 && SS.GW.gs.entities == 1) {
                e = SK.GetEntity(SS.GW.gs.entity[0]);
            }
            SS.GW.ClearSelection();

            hGroup hg = e ? e->group : SS.GW.activeGroup;
            Group *g = SK.GetGroup(hg);
            if(g->type != Group::Type::LINKED) {
                Error(_("To use this command, select a point or other "
                        "entity from an linked part, or make a link "
                        "group the active group."));
                break;
            }

            SS.UndoRemember();
            // Rotate by ninety degrees about the coordinate axis closest
            // to the screen normal.
            Vector norm = SS.GW.projRight.Cross(SS.GW.projUp);
            norm = norm.ClosestOrtho();
            norm = norm.WithMagnitude(1);
            Quaternion qaa = Quaternion::From(norm, PI/2);

            g->TransformImportedBy(Vector::From(0, 0, 0), qaa);

            // and regenerate as necessary.
            SS.MarkGroupDirty(hg);
            break;
        }

        case Command::SNAP_TO_GRID: {
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("No workplane is active. Activate a workplane "
                        "(with Sketch -> In Workplane) to define the plane "
                        "for the snap grid."));
                break;
            }
            SS.GW.GroupSelection();
            if(SS.GW.gs.points == 0 && SS.GW.gs.constraintLabels == 0) {
                Error(_("Can't snap these items to grid; select points, "
                        "text comments, or constraints with a label. "
                        "To snap a line, select its endpoints."));
                break;
            }
            SS.UndoRemember();

            List<Selection> *ls = &(SS.GW.selection);
            for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
                if(s->entity.v) {
                    hEntity hp = s->entity;
                    Entity *ep = SK.GetEntity(hp);
                    if(!ep->IsPoint()) continue;

                    Vector p = ep->PointGetNum();
                    ep->PointForceTo(SS.GW.SnapToGrid(p));
                    SS.GW.pending.points.Add(&hp);
                    SS.MarkGroupDirty(ep->group);
                } else if(s->constraint.v) {
                    Constraint *c = SK.GetConstraint(s->constraint);
                    std::vector<Vector> refs;
                    c->GetReferencePoints(SS.GW.GetCamera(), &refs);
                    c->disp.offset = c->disp.offset.Plus(SS.GW.SnapToGrid(refs[0]).Minus(refs[0]));
                }
            }
            // Regenerate, with these points marked as dragged so that they
            // get placed as close as possible to our snap grid.
            SS.GW.ClearPending();

            SS.GW.ClearSelection();
            SS.GW.Invalidate();
            break;
        }

        case Command::UNDO:
            SS.UndoUndo();
            break;

        case Command::REDO:
            SS.UndoRedo();
            break;

        case Command::REGEN_ALL:
            SS.images.clear();
            SS.ReloadAllLinked(SS.saveFile);
            SS.GenerateAll(SolveSpaceUI::Generate::UNTIL_ACTIVE);
            SS.ScheduleShowTW();
            break;

        case Command::EDIT_LINE_STYLES:
            SS.TW.GoToScreen(TextWindow::Screen::LIST_OF_STYLES);
            SS.GW.ForceTextWindowShown();
            SS.ScheduleShowTW();
            break;
        case Command::VIEW_PROJECTION:
            SS.TW.GoToScreen(TextWindow::Screen::EDIT_VIEW);
            SS.GW.ForceTextWindowShown();
            SS.ScheduleShowTW();
            break;
        case Command::CONFIGURATION:
            SS.TW.GoToScreen(TextWindow::Screen::CONFIGURATION);
            SS.GW.ForceTextWindowShown();
            SS.ScheduleShowTW();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
}

void GraphicsWindow::MenuRequest(Command id) {
    const char *s;
    switch(id) {
        case Command::SEL_WORKPLANE: {
            SS.GW.GroupSelection();
            Group *g = SK.GetGroup(SS.GW.activeGroup);

            if(SS.GW.gs.n == 1 && SS.GW.gs.workplanes == 1) {
                // A user-selected workplane
                g->activeWorkplane = SS.GW.gs.entity[0];
            } else if(g->type == Group::Type::DRAWING_WORKPLANE) {
                // The group's default workplane
                g->activeWorkplane = g->h.entity(0);
                MessageAndRun([] {
                    // Align the view with the selected workplane
                    SS.GW.ClearSuper();
                    SS.GW.AnimateOntoWorkplane();
                }, _("No workplane selected. Activating default workplane "
                     "for this group."));
            } else {
                Error(_("No workplane is selected, and the active group does "
                        "not have a default workplane. Try selecting a "
                        "workplane, or activating a sketch-in-new-workplane "
                        "group."));
            }
            break;
        }
        case Command::FREE_IN_3D:
            SS.GW.SetWorkplaneFreeIn3d();
            SS.GW.EnsureValidActives();
            SS.ScheduleShowTW();
            SS.GW.Invalidate();
            break;

        case Command::TANGENT_ARC:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                SS.GW.MakeTangentArc();
            } else if(SS.GW.gs.n != 0) {
                Error(_("Bad selection for tangent arc at point. Select a "
                        "single point, or select nothing to set up arc "
                        "parameters."));
            } else {
                SS.TW.GoToScreen(TextWindow::Screen::TANGENT_ARC);
                SS.GW.ForceTextWindowShown();
                SS.ScheduleShowTW();
                SS.GW.Invalidate(); // repaint toolbar
            }
            break;

        case Command::ARC: s = _("click point on arc (draws anti-clockwise)"); goto c;
        case Command::DATUM_POINT: s = _("click to place datum point"); goto c;
        case Command::LINE_SEGMENT: s = _("click first point of line segment"); goto c;
        case Command::CONSTR_SEGMENT:
            s = _("click first point of construction line segment"); goto c;
        case Command::CUBIC: s = _("click first point of cubic segment"); goto c;
        case Command::CIRCLE: s = _("click center of circle"); goto c;
        case Command::WORKPLANE: s = _("click origin of workplane"); goto c;
        case Command::RECTANGLE: s = _("click one corner of rectangle"); goto c;
        case Command::TTF_TEXT: s = _("click top left of text"); goto c;
        case Command::IMAGE:
            if(!SS.ReloadLinkedImage(SS.saveFile, &SS.GW.pending.filename,
                                     /*canCancel=*/true)) {
                return;
            }
            s = _("click top left of image"); goto c;
c:
            SS.GW.pending.operation = GraphicsWindow::Pending::COMMAND;
            SS.GW.pending.command = id;
            SS.GW.pending.description = s;
            SS.ScheduleShowTW();
            SS.GW.Invalidate(); // repaint toolbar
            break;

        case Command::CONSTRUCTION: {
            SS.GW.GroupSelection();
            if(SS.GW.gs.entities == 0) {
                Error(_("No entities are selected. Select entities before "
                        "trying to toggle their construction state."));
                break;
            }
            SS.UndoRemember();
            int i;
            for(i = 0; i < SS.GW.gs.entities; i++) {
                hEntity he = SS.GW.gs.entity[i];
                if(!he.isFromRequest()) continue;
                Request *r = SK.GetRequest(he.request());
                r->construction = !(r->construction);
                SS.MarkGroupDirty(r->group);
            }
            SS.GW.ClearSelection();
            break;
        }

        case Command::SPLIT_CURVES:
            SS.GW.SplitLinesOrCurves();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
}

void GraphicsWindow::ClearSuper() {
    if(window) window->HideEditor();
    ClearPending();
    ClearSelection();
    hover.Clear();
    EnsureValidActives();
}

void GraphicsWindow::ToggleBool(bool *v) {
    *v = !*v;

    // The faces are shown as special stippling on the shaded triangle mesh,
    // so not meaningful to show them and hide the shaded.
    if(!showShaded) showFaces = false;

    // If the mesh or edges were previously hidden, they haven't been generated,
    // and if we are going to show them, we need to generate them first.
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    if(*v && (g->displayOutlines.l.IsEmpty() && (v == &showEdges || v == &showOutlines))) {
        SS.GenerateAll(SolveSpaceUI::Generate::UNTIL_ACTIVE);
    }

    Invalidate(/*clearPersistent=*/true);
    SS.ScheduleShowTW();
}

bool GraphicsWindow::SuggestLineConstraint(hRequest request, Constraint::Type *type) {
    if(!(LockedInWorkplane() && SS.automaticLineConstraints))
        return false;

    Entity *ptA = SK.GetEntity(request.entity(1)),
           *ptB = SK.GetEntity(request.entity(2));

    Expr *au, *av, *bu, *bv;

    ptA->PointGetExprsInWorkplane(ActiveWorkplane(), &au, &av);
    ptB->PointGetExprsInWorkplane(ActiveWorkplane(), &bu, &bv);

    double du = au->Minus(bu)->Eval();
    double dv = av->Minus(bv)->Eval();

    const double TOLERANCE_RATIO = 0.02;
    if(fabs(dv) > LENGTH_EPS && fabs(du / dv) < TOLERANCE_RATIO) {
        *type = Constraint::Type::VERTICAL;
        return true;
    } else if(fabs(du) > LENGTH_EPS && fabs(dv / du) < TOLERANCE_RATIO) {
        *type = Constraint::Type::HORIZONTAL;
        return true;
    }
    return false;
}
