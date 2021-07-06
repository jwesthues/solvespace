//-----------------------------------------------------------------------------
// Declarations relating to our user interface, in both the graphics and
// text browser window.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_UI_H
#define SOLVESPACE_UI_H

class Locale {
public:
    std::string language;
    std::string region;
    uint16_t    lcid;
    std::string displayName;

    std::string Culture() const {
        return language + "-" + region;
    }
};

struct LocaleLess {
    bool operator()(const Locale &a, const Locale &b) const {
        return a.language < b.language ||
            (a.language == b.language && a.region < b.region);
    }
};

const std::set<Locale, LocaleLess> &Locales();
bool SetLocale(const std::string &name);
bool SetLocale(uint16_t lcid);

const std::string &Translate(const char *msgid);
const std::string &Translate(const char *msgctxt, const char *msgid);
const std::string &TranslatePlural(const char *msgid, unsigned n);
const std::string &TranslatePlural(const char *msgctxt, const char *msgid, unsigned n);

inline const char *N_(const char *msgid) {
    return msgid;
}
inline const char *CN_(const char *msgctxt, const char *msgid) {
    return msgid;
}
#if defined(LIBRARY)
inline const char *_(const char *msgid) {
    return msgid;
}
inline const char *C_(const char *msgctxt, const char *msgid) {
    return msgid;
}
#else
inline const char *_(const char *msgid) {
    return Translate(msgid).c_str();
}
inline const char *C_(const char *msgctxt, const char *msgid) {
    return Translate(msgctxt, msgid).c_str();
}
#endif

// This table describes the top-level menus in the graphics window.
enum class Command : uint32_t {
    NONE = 0,
    // File
    NEW = 100,
    OPEN,
    OPEN_RECENT,
    SAVE,
    SAVE_AS,
    EXPORT_IMAGE,
    EXPORT_MESH,
    EXPORT_SURFACES,
    EXPORT_VIEW,
    EXPORT_SECTION,
    EXPORT_WIREFRAME,
    IMPORT,
    EXIT,
    // View
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_TO_FIT,
    SHOW_GRID,
    DIM_SOLID_MODEL,
    PERSPECTIVE_PROJ,
    ONTO_WORKPLANE,
    NEAREST_ORTHO,
    NEAREST_ISO,
    CENTER_VIEW,
    SHOW_TOOLBAR,
    SHOW_TEXT_WND,
    UNITS_INCHES,
    UNITS_MM,
    UNITS_METERS,
    FULL_SCREEN,
    // Edit
    UNDO,
    REDO,
    CUT,
    COPY,
    PASTE,
    PASTE_TRANSFORM,
    DELETE,
    SELECT_CHAIN,
    SELECT_ALL,
    SNAP_TO_GRID,
    ROTATE_90,
    UNSELECT_ALL,
    REGEN_ALL,
    EDIT_LINE_STYLES,
    VIEW_PROJECTION,
    CONFIGURATION,
    // Request
    SEL_WORKPLANE,
    FREE_IN_3D,
    DATUM_POINT,
    WORKPLANE,
    LINE_SEGMENT,
    CONSTR_SEGMENT,
    CIRCLE,
    ARC,
    RECTANGLE,
    CUBIC,
    TTF_TEXT,
    IMAGE,
    SPLIT_CURVES,
    TANGENT_ARC,
    CONSTRUCTION,
    // Group
    GROUP_3D,
    GROUP_WRKPL,
    GROUP_EXTRUDE,
    GROUP_HELIX,
    GROUP_LATHE,
    GROUP_MIRROR,
    GROUP_REVOLVE,
    GROUP_ROT,
    GROUP_TRANS,
    GROUP_LINK,
    GROUP_RECENT,
    // Constrain
    DISTANCE_DIA,
    REF_DISTANCE,
    ANGLE,
    REF_ANGLE,
    OTHER_ANGLE,
    REFERENCE,
    EQUAL,
    RATIO,
    DIFFERENCE,
    ON_ENTITY,
    SYMMETRIC,
    AT_MIDPOINT,
    HORIZONTAL,
    VERTICAL,
    PARALLEL,
    PERPENDICULAR,
    ORIENTED_SAME,
    WHERE_DRAGGED,
    COMMENT,
    // Analyze
    VOLUME,
    AREA,
    PERIMETER,
    INTERFERENCE,
    NAKED_EDGES,
    SHOW_DOF,
    CENTER_OF_MASS,
    TRACE_PT,
    STOP_TRACING,
    STEP_DIM,
    // Help
    LOCALE,
    WEBSITE,
    ABOUT,
};

class Button;

class TextWindow {
public:
    enum {
        MAX_COLS = 100,
        MIN_COLS = 45,
        MAX_ROWS = 4000
    };

    typedef struct {
        char      c;
        RgbaColor color;
    } Color;
    static const Color fgColors[];
    static const Color bgColors[];

    float bgColorTable[256*3];
    float fgColorTable[256*3];

    enum {
        CHAR_WIDTH_    = 9,
        CHAR_HEIGHT    = 16,
        LINE_HEIGHT    = 20,
        LEFT_MARGIN    = 6,
    };

#define CHECK_FALSE "\xEE\x80\x80" // U+E000
#define CHECK_TRUE  "\xEE\x80\x81"
#define RADIO_FALSE "\xEE\x80\x82"
#define RADIO_TRUE  "\xEE\x80\x83"

    int scrollPos;      // The scrollbar position, in half-row units
    int halfRows;       // The height of our window, in half-row units

    uint32_t text[MAX_ROWS][MAX_COLS];
    typedef void LinkFunction(int link, uint32_t v);
    enum { NOT_A_LINK = 0 };
    struct {
        char            fg;
        char            bg;
        RgbaColor       bgRgb;
        int             link;
        uint32_t        data;
        LinkFunction   *f;
        LinkFunction   *h;
    }       meta[MAX_ROWS][MAX_COLS];
    int hoveredRow, hoveredCol;

    int top[MAX_ROWS]; // in half-line units, or -1 for unused
    int rows;

    Platform::WindowRef window;
    std::shared_ptr<ViewportCanvas> canvas;

    void Draw(Canvas *canvas);

    void Paint();
    void MouseEvent(bool isClick, bool leftDown, double x, double y);
    void MouseLeave();
    void ScrollbarEvent(double newPos);

    enum DrawOrHitHow : uint32_t {
        PAINT = 0,
        HOVER = 1,
        CLICK = 2
    };
    void DrawOrHitTestIcons(UiCanvas *canvas, DrawOrHitHow how,
                            double mx, double my);
    Button *hoveredButton;

    Vector HsvToRgb(Vector hsv);
    std::shared_ptr<Pixmap> HsvPattern2d(int w, int h);
    std::shared_ptr<Pixmap> HsvPattern1d(double hue, double sat, int w, int h);
    void ColorPickerDone();
    bool DrawOrHitTestColorPicker(UiCanvas *canvas, DrawOrHitHow how,
                                  bool leftDown, double x, double y);

    void Init();
    void MakeColorTable(const Color *in, float *out);
    void Printf(bool half, const char *fmt, ...);
    void ClearScreen();

    void Show();
    void Resize();

    // State for the screen that we are showing in the text window.
    enum class Screen : uint32_t {
        LIST_OF_GROUPS      = 0,
        GROUP_INFO          = 1,
        GROUP_SOLVE_INFO    = 2,
        CONFIGURATION       = 3,
        STEP_DIMENSION      = 4,
        LIST_OF_STYLES      = 5,
        STYLE_INFO          = 6,
        PASTE_TRANSFORMED   = 7,
        EDIT_VIEW           = 8,
        TANGENT_ARC         = 9
    };
    typedef struct {
        Screen  screen;

        hGroup      group;
        hStyle      style;

        hConstraint constraint;

        struct {
            int         times;
            Vector      trans;
            double      theta;
            Vector      origin;
            double      scale;
        }           paste;
    } ShownState;
    ShownState shown;

    enum class Edit : uint32_t {
        NOTHING               = 0,
        // For multiple groups
        TIMES_REPEATED        = 1,
        GROUP_NAME            = 2,
        GROUP_SCALE           = 3,
        GROUP_COLOR           = 4,
        GROUP_OPACITY         = 5,
        // For the configuration screen
        LIGHT_DIRECTION       = 100,
        LIGHT_INTENSITY       = 101,
        COLOR                 = 102,
        CHORD_TOLERANCE       = 103,
        MAX_SEGMENTS          = 104,
        CAMERA_TANGENT        = 105,
        GRID_SPACING          = 106,
        DIGITS_AFTER_DECIMAL  = 107,
        DIGITS_AFTER_DECIMAL_DEGREE = 108,
        EXPORT_SCALE          = 109,
        EXPORT_OFFSET         = 110,
        CANVAS_SIZE           = 111,
        G_CODE_DEPTH          = 112,
        G_CODE_PASSES         = 113,
        G_CODE_FEED           = 114,
        G_CODE_PLUNGE_FEED    = 115,
        AUTOSAVE_INTERVAL     = 116,
        LIGHT_AMBIENT         = 117,
        FIND_CONSTRAINT_TIMEOUT = 118,
        // For TTF text
        TTF_TEXT              = 300,
        // For the step dimension screen
        STEP_DIM_FINISH       = 400,
        STEP_DIM_STEPS        = 401,
        // For the styles stuff
        STYLE_WIDTH           = 500,
        STYLE_TEXT_HEIGHT     = 501,
        STYLE_TEXT_ANGLE      = 502,
        STYLE_COLOR           = 503,
        STYLE_FILL_COLOR      = 504,
        STYLE_NAME            = 505,
        BACKGROUND_COLOR      = 506,
        STYLE_STIPPLE_PERIOD  = 508,
        // For paste transforming
        PASTE_TIMES_REPEATED  = 600,
        PASTE_ANGLE           = 601,
        PASTE_SCALE           = 602,
        // For view
        VIEW_SCALE            = 700,
        VIEW_ORIGIN           = 701,
        VIEW_PROJ_RIGHT       = 702,
        VIEW_PROJ_UP          = 703,
        // For tangent arc
        TANGENT_ARC_RADIUS    = 800
    };
    struct {
        bool        showAgain;
        Edit        meaning;
        int         i;
        hGroup      group;
        hRequest    request;
        hStyle      style;
    } edit;

    static void ReportHowGroupSolved(hGroup hg);

    struct {
        int     halfRow;
        int     col;

        struct {
            RgbaColor rgb;
            double    h, s, v;
            bool      show;
            bool      picker1dActive;
            bool      picker2dActive;
        }       colorPicker;
    } editControl;

    void HideEditControl();
    void ShowEditControl(int col, const std::string &str, int halfRow = -1);
    void ShowEditControlWithColorPicker(int col, RgbaColor rgb);

    void ClearSuper();

    void ShowHeader(bool withNav);
    // These are self-contained screens, that show some information about
    // the sketch.
    void ShowListOfGroups();
    void ShowGroupInfo();
    void ShowGroupSolveInfo();
    void ShowConfiguration();
    void ShowListOfStyles();
    void ShowStyleInfo();
    void ShowStepDimension();
    void ShowPasteTransformed();
    void ShowEditView();
    void ShowTangentArc();
    // Special screen, based on selection
    void DescribeSelection();

    void GoToScreen(Screen screen);

    // All of these are callbacks from the GUI code; first from when
    // we're describing an entity
    static void ScreenEditTtfText(int link, uint32_t v);
    static void ScreenSetTtfFont(int link, uint32_t v);
    static void ScreenUnselectAll(int link, uint32_t v);

    // when we're describing a constraint
    static void ScreenConstraintToggleReference(int link, uint32_t v);
    static void ScreenConstraintShowAsRadius(int link, uint32_t v);

    // and the rest from the stuff in textscreens.cpp
    static void ScreenSelectGroup(int link, uint32_t v);
    static void ScreenActivateGroup(int link, uint32_t v);
    static void ScreenToggleGroupShown(int link, uint32_t v);
    static void ScreenHowGroupSolved(int link, uint32_t v);
    static void ScreenShowGroupsSpecial(int link, uint32_t v);
    static void ScreenDeleteGroup(int link, uint32_t v);

    static void ScreenHoverGroupWorkplane(int link, uint32_t v);
    static void ScreenHoverRequest(int link, uint32_t v);
    static void ScreenHoverEntity(int link, uint32_t v);
    static void ScreenHoverConstraint(int link, uint32_t v);
    static void ScreenSelectRequest(int link, uint32_t v);
    static void ScreenSelectEntity(int link, uint32_t v);
    static void ScreenSelectConstraint(int link, uint32_t v);

    static void ScreenChangeGroupOption(int link, uint32_t v);
    static void ScreenColor(int link, uint32_t v);
    static void ScreenOpacity(int link, uint32_t v);

    static void ScreenShowListOfStyles(int link, uint32_t v);
    static void ScreenShowStyleInfo(int link, uint32_t v);
    static void ScreenDeleteStyle(int link, uint32_t v);
    static void ScreenChangeStylePatternType(int link, uint32_t v);
    static void ScreenChangeStyleYesNo(int link, uint32_t v);
    static void ScreenCreateCustomStyle(int link, uint32_t v);
    static void ScreenLoadFactoryDefaultStyles(int link, uint32_t v);
    static void ScreenAssignSelectionToStyle(int link, uint32_t v);

    static void ScreenShowConfiguration(int link, uint32_t v);
    static void ScreenShowEditView(int link, uint32_t v);
    static void ScreenGoToWebsite(int link, uint32_t v);

    static void ScreenChangeFixExportColors(int link, uint32_t v);
    static void ScreenChangeExportBackgroundColor(int link, uint32_t v);
    static void ScreenChangeBackFaces(int link, uint32_t v);
    static void ScreenChangeShowContourAreas(int link, uint32_t v);
    static void ScreenChangeCheckClosedContour(int link, uint32_t v);
    static void ScreenChangeTurntableNav(int link, uint32_t v);
    static void ScreenChangeImmediatelyEditDimension(int link, uint32_t v);
    static void ScreenChangeAutomaticLineConstraints(int link, uint32_t v);
    static void ScreenChangePwlCurves(int link, uint32_t v);
    static void ScreenChangeCanvasSizeAuto(int link, uint32_t v);
    static void ScreenChangeCanvasSize(int link, uint32_t v);
    static void ScreenChangeShadedTriangles(int link, uint32_t v);

    static void ScreenAllowRedundant(int link, uint32_t v);

    struct {
        bool    isDistance;
        double  finish;
        int     steps;

        Platform::TimerRef timer;
        int64_t time;
        int     step;
    } stepDim;
    static void ScreenStepDimSteps(int link, uint32_t v);
    static void ScreenStepDimFinish(int link, uint32_t v);
    static void ScreenStepDimGo(int link, uint32_t v);

    static void ScreenChangeTangentArc(int link, uint32_t v);

    static void ScreenPasteTransformed(int link, uint32_t v);

    static void ScreenHome(int link, uint32_t v);

    // These ones do stuff with the edit control
    static void ScreenChangeExprA(int link, uint32_t v);
    static void ScreenChangeGroupName(int link, uint32_t v);
    static void ScreenChangeGroupScale(int link, uint32_t v);
    static void ScreenChangeLightDirection(int link, uint32_t v);
    static void ScreenChangeLightIntensity(int link, uint32_t v);
    static void ScreenChangeLightAmbient(int link, uint32_t v);
    static void ScreenChangeColor(int link, uint32_t v);
    static void ScreenChangeChordTolerance(int link, uint32_t v);
    static void ScreenChangeMaxSegments(int link, uint32_t v);
    static void ScreenChangeExportChordTolerance(int link, uint32_t v);
    static void ScreenChangeExportMaxSegments(int link, uint32_t v);
    static void ScreenChangeCameraTangent(int link, uint32_t v);
    static void ScreenChangeGridSpacing(int link, uint32_t v);
    static void ScreenChangeDigitsAfterDecimal(int link, uint32_t v);
    static void ScreenChangeDigitsAfterDecimalDegree(int link, uint32_t v);
    static void ScreenChangeUseSIPrefixes(int link, uint32_t v);
    static void ScreenChangeExportScale(int link, uint32_t v);
    static void ScreenChangeExportOffset(int link, uint32_t v);
    static void ScreenChangeGCodeParameter(int link, uint32_t v);
    static void ScreenChangeAutosaveInterval(int link, uint32_t v);
    static void ScreenChangeFindConstraintTimeout(int link, uint32_t v);
    static void ScreenChangeStyleName(int link, uint32_t v);
    static void ScreenChangeStyleMetric(int link, uint32_t v);
    static void ScreenChangeStyleTextAngle(int link, uint32_t v);
    static void ScreenChangeStyleColor(int link, uint32_t v);
    static void ScreenChangeBackgroundColor(int link, uint32_t v);
    static void ScreenChangePasteTransformed(int link, uint32_t v);
    static void ScreenChangeViewScale(int link, uint32_t v);
    static void ScreenChangeViewToFullScale(int link, uint32_t v);
    static void ScreenChangeViewOrigin(int link, uint32_t v);
    static void ScreenChangeViewProjection(int link, uint32_t v);

    bool EditControlDoneForStyles(const std::string &s);
    bool EditControlDoneForConfiguration(const std::string &s);
    bool EditControlDoneForPaste(const std::string &s);
    bool EditControlDoneForView(const std::string &s);
    void EditControlDone(std::string s);
};

class GraphicsWindow {
public:
    void Init();

    Platform::WindowRef   window;

    void PopulateMainMenu();
    void PopulateRecentFiles();

    Platform::KeyboardEvent AcceleratorForCommand(Command id);
    void ActivateCommand(Command id);

    static void MenuView(Command id);
    static void MenuEdit(Command id);
    static void MenuRequest(Command id);
    void DeleteSelection();
    void CopySelection();
    void PasteClipboard(Vector trans, double theta, double scale);
    static void MenuClipboard(Command id);

    Platform::MenuRef openRecentMenu;
    Platform::MenuRef linkRecentMenu;

    Platform::MenuItemRef showGridMenuItem;
    Platform::MenuItemRef dimSolidModelMenuItem;
    Platform::MenuItemRef perspectiveProjMenuItem;
    Platform::MenuItemRef showToolbarMenuItem;
    Platform::MenuItemRef showTextWndMenuItem;
    Platform::MenuItemRef fullScreenMenuItem;

    Platform::MenuItemRef unitsMmMenuItem;
    Platform::MenuItemRef unitsMetersMenuItem;
    Platform::MenuItemRef unitsInchesMenuItem;

    Platform::MenuItemRef inWorkplaneMenuItem;
    Platform::MenuItemRef in3dMenuItem;

    Platform::MenuItemRef undoMenuItem;
    Platform::MenuItemRef redoMenuItem;

    std::shared_ptr<ViewportCanvas> canvas;
    std::shared_ptr<BatchCanvas>    persistentCanvas;
    bool persistentDirty;

    // These parameters define the map from 2d screen coordinates to the
    // coordinates of the 3d sketch points. We will use an axonometric
    // projection.
    Vector  offset;
    Vector  projRight;
    Vector  projUp;
    double  scale;
    struct {
        bool    mouseDown;
        Vector  offset;
        Vector  projRight;
        Vector  projUp;
        Point2d mouse;
        Point2d mouseOnButtonDown;
        Vector  marqueePoint;
        bool    startedMoving;
    }       orig;
    // We need to detect when the projection is changed to invalidate
    // caches for drawn items.
    struct {
        Vector  offset;
        Vector  projRight;
        Vector  projUp;
        double  scale;
    }       cached;

    // Most recent mouse position, updated every time the mouse moves.
    Point2d currentMousePosition;

    // When the user is dragging a point, don't solve multiple times without
    // allowing a paint in between. The extra solves are wasted if they're
    // not displayed.
    bool    havePainted;

    // Some state for the context menu.
    struct {
        bool        active;
    }       context;

    Camera GetCamera() const;
    Lighting GetLighting() const;

    void NormalizeProjectionVectors();
    Point2d ProjectPoint(Vector p);
    Vector ProjectPoint3(Vector p);
    Vector ProjectPoint4(Vector p, double *w);
    Vector UnProjectPoint(Point2d p);
    Vector UnProjectPoint3(Vector p);

    Platform::TimerRef animateTimer;
    void AnimateOnto(Quaternion quatf, Vector offsetf);
    void AnimateOntoWorkplane();

    Vector VectorFromProjs(Vector rightUpForward);
    void HandlePointForZoomToFit(Vector p, Point2d *pmax, Point2d *pmin,
                                 double *wmin, bool usePerspective,
                                 const Camera &camera);
    void LoopOverPoints(const std::vector<Entity *> &entities,
                        const std::vector<Constraint *> &constraints,
                        const std::vector<hEntity> &faces,
                        Point2d *pmax, Point2d *pmin,
                        double *wmin, bool usePerspective, bool includeMesh,
                        const Camera &camera);
    void ZoomToFit(bool includingInvisibles = false, bool useSelection = false);
    double ZoomToFit(const Camera &camera,
                     bool includingInvisibles = false, bool useSelection = false);

    hGroup  activeGroup;
    void EnsureValidActives();
    bool LockedInWorkplane();
    void SetWorkplaneFreeIn3d();
    hEntity ActiveWorkplane();
    void ForceTextWindowShown();

    // Operations that must be completed by doing something with the mouse
    // are noted here.
    enum class Pending : uint32_t {
        NONE                        = 0,
        COMMAND                     = 1,
        DRAGGING_POINTS             = 2,
        DRAGGING_NEW_POINT          = 3,
        DRAGGING_NEW_LINE_POINT     = 4,
        DRAGGING_NEW_CUBIC_POINT    = 5,
        DRAGGING_NEW_ARC_POINT      = 6,
        DRAGGING_CONSTRAINT         = 7,
        DRAGGING_RADIUS             = 8,
        DRAGGING_NORMAL             = 9,
        DRAGGING_NEW_RADIUS         = 10,
        DRAGGING_MARQUEE            = 11,
    };

    struct {
        Pending              operation;
        Command              command;

        hRequest             request;
        hEntity              point;
        List<hEntity>        points;
        List<hRequest>       requests;
        hEntity              circle;
        hEntity              normal;
        hConstraint          constraint;

        const char          *description;
        Platform::Path       filename;

        bool                 hasSuggestion;
        Constraint::Type     suggestion;
    } pending;
    void ClearPending(bool scheduleShowTW = true);
    bool IsFromPending(hRequest r);
    void AddToPending(hRequest r);
    void ReplacePending(hRequest before, hRequest after);

    // The constraint that is being edited with the on-screen textbox.
    hConstraint constraintBeingEdited;

    bool SuggestLineConstraint(hRequest lineSegment, ConstraintBase::Type *type);

    Vector SnapToGrid(Vector p);
    Vector SnapToEntityByScreenPoint(Point2d pp, hEntity he);
    bool ConstrainPointByHovered(hEntity pt, const Point2d *projected = NULL);
    void DeleteTaggedRequests();
    hRequest AddRequest(Request::Type type, bool rememberForUndo);
    hRequest AddRequest(Request::Type type);

    class ParametricCurve {
    public:
        bool isLine; // else circle
        Vector p0, p1;
        Vector u, v;
        double r, theta0, theta1, dtheta;

        void MakeFromEntity(hEntity he, bool reverse);
        Vector PointAt(double t);
        Vector TangentAt(double t);
        double LengthForAuto();

        void CreateRequestTrimmedTo(double t, bool reuseOrig,
            hEntity orig, hEntity arc, bool arcFinish, bool pointf);
        void ConstrainPointIfCoincident(hEntity hpt);
    };
    void MakeTangentArc();
    void SplitLinesOrCurves();
    hEntity SplitEntity(hEntity he, Vector pinter);
    hEntity SplitLine(hEntity he, Vector pinter);
    hEntity SplitCircle(hEntity he, Vector pinter);
    hEntity SplitCubic(hEntity he, Vector pinter);
    void ReplacePointInConstraints(hEntity oldpt, hEntity newpt);
    void RemoveConstraintsForPointBeingDeleted(hEntity hpt);
    void FixConstraintsForRequestBeingDeleted(hRequest hr);
    void FixConstraintsForPointBeingDeleted(hEntity hpt);
    void EditConstraint(hConstraint constraint);

    // A selected entity.
    class Selection {
    public:
        int         tag;

        hEntity     entity;
        hConstraint constraint;
        bool        emphasized;

        void Draw(bool isHovered, Canvas *canvas);

        void Clear();
        bool IsEmpty();
        bool Equals(Selection *b);
        bool HasEndpoints();
    };

    // A hovered entity, with its location relative to the cursor.
    class Hover {
    public:
        int         zIndex;
        double      distance;
        double      depth;
        Selection   selection;
    };

    List<Hover> hoverList;
    Selection hover;
    bool hoverWasSelectedOnMousedown;
    List<Selection> selection;

    Selection ChooseFromHoverToSelect();
    Selection ChooseFromHoverToDrag();
    void HitTestMakeSelection(Point2d mp);
    void ClearSelection();
    void ClearNonexistentSelectionItems();
    /// This structure is filled by a call to GroupSelection().
    struct {
        std::vector<hEntity>     point;
        std::vector<hEntity>     entity;
        std::vector<hEntity>     anyNormal;
        std::vector<hEntity>     vector;
        std::vector<hEntity>     face;
        std::vector<hConstraint> constraint;
        int         points;
        int         entities;
        int         workplanes;
        int         faces;
        int         lineSegments;
        int         circlesOrArcs;
        int         arcs;
        int         cubics;
        int         periodicCubics;
        int         anyNormals;
        int         vectors;
        int         constraints;
        int         stylables;
        int         constraintLabels;
        int         withEndpoints;
        int         n;                 ///< Number of selected items
    } gs;
    void GroupSelection();
    bool IsSelected(Selection *s);
    bool IsSelected(hEntity he);
    void MakeSelected(hEntity he);
    void MakeSelected(hConstraint hc);
    void MakeSelected(Selection *s);
    void MakeUnselected(hEntity he, bool coincidentPointTrick);
    void MakeUnselected(Selection *s, bool coincidentPointTrick);
    void SelectByMarquee();
    void ClearSuper();

    // The toolbar, in toolbar.cpp
    bool ToolbarDrawOrHitTest(int x, int y, UiCanvas *canvas,
                              Command *hitCommand, int *hitX, int *hitY);
    void ToolbarDraw(UiCanvas *canvas);
    bool ToolbarMouseMoved(int x, int y);
    bool ToolbarMouseDown(int x, int y);
    Command toolbarHovered;

    // This sets what gets displayed.
    bool    showWorkplanes;
    bool    showNormals;
    bool    showPoints;
    bool    showConstruction;
    bool    showConstraints;
    bool    showTextWindow;
    bool    showShaded;
    bool    showEdges;
    bool    showOutlines;
    bool    showFaces;
    bool    showMesh;
    void ToggleBool(bool *v);

    enum class DrawOccludedAs { INVISIBLE, STIPPLED, VISIBLE };
    DrawOccludedAs drawOccludedAs;

    bool    showSnapGrid;
    bool    dimSolidModel;
    void DrawSnapGrid(Canvas *canvas);

    void AddPointToDraggedList(hEntity hp);
    void StartDraggingByEntity(hEntity he);
    void StartDraggingBySelection();
    void UpdateDraggedNum(Vector *pos, double mx, double my);
    void UpdateDraggedPoint(hEntity hp, double mx, double my);

    void Invalidate(bool clearPersistent = false);
    void DrawEntities(Canvas *canvas, bool persistent);
    void DrawPersistent(Canvas *canvas);
    void Draw(Canvas *canvas);
    void Paint();

    bool MouseEvent(Platform::MouseEvent event);
    void MouseMoved(double x, double y, bool leftDown, bool middleDown,
                    bool rightDown, bool shiftDown, bool ctrlDown);
    void MouseLeftDown(double x, double y, bool shiftDown, bool ctrlDown);
    void MouseLeftUp(double x, double y, bool shiftDown, bool ctrlDown);
    void MouseLeftDoubleClick(double x, double y);
    void MouseMiddleOrRightDown(double x, double y);
    void MouseRightUp(double x, double y);
    void MouseScroll(double x, double y, double delta);
    void MouseLeave();
    bool KeyboardEvent(Platform::KeyboardEvent event);
    void EditControlDone(const std::string &s);

    int64_t last6DofTime;
    hGroup last6DofGroup;
    void SixDofEvent(Platform::SixDofEvent event);
};


#endif
