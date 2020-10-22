//-----------------------------------------------------------------------------
// Helper functions for the text-based browser window.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

class Button {
public:
    virtual std::string Tooltip() = 0;
    virtual void Draw(UiCanvas *uiCanvas, int x, int y, bool asHovered) = 0;
    virtual int AdvanceWidth() = 0;
    virtual void Click() = 0;
};

class SpacerButton : public Button {
public:
    std::string Tooltip() override { return ""; }

    void Draw(UiCanvas *uiCanvas, int x, int y, bool asHovered) override {
        // Draw a darker-grey spacer in between the groups of icons.
        uiCanvas->DrawRect(x, x + 4, y, y - 24,
                           /*fillColor=*/{ 45, 45, 45, 255 },
                           /*outlineColor=*/{});
    }

    int AdvanceWidth() override { return 12; }

    void Click() override {}
};

class ShowHideButton : public Button {
public:
    bool        *variable;
    std::string tooltip;
    std::string iconName;
    std::shared_ptr<Pixmap> icon;

    ShowHideButton(bool *variable, std::string iconName, std::string tooltip)
            : variable(variable), tooltip(tooltip), iconName(iconName) {}

    std::string Tooltip() override {
        return ((*variable) ? "Hide " : "Show ") + tooltip;
    }

    void Draw(UiCanvas *uiCanvas, int x, int y, bool asHovered) override {
        if(icon == NULL) {
            icon = LoadPng("icons/text-window/" + iconName + ".png");
        }

        uiCanvas->DrawPixmap(icon, x, y - 24);
        if(asHovered) {
            uiCanvas->DrawRect(x - 2, x + 26, y + 2, y - 26,
                               /*fillColor=*/{ 255, 255, 0, 75 },
                               /*outlineColor=*/{});
        }
        if(!*(variable)) {
            int s = 0, f = 24;
            RgbaColor color = { 255, 0, 0, 150 };
            uiCanvas->DrawLine(x+s, y-s, x+f, y-f, color, 2);
            uiCanvas->DrawLine(x+s, y-f, x+f, y-s, color, 2);
        }
    }

    int AdvanceWidth() override { return 32; }

    void Click() override { SS.GW.ToggleBool(variable); }
};

class FacesButton : public ShowHideButton {
public:
    FacesButton()
        : ShowHideButton(&(SS.GW.showFaces), "faces", "") {}

    std::string Tooltip() override {
        if(*variable) {
            return "Don't make faces selectable with mouse";
        } else {
            return "Make faces selectable with mouse";
        }
    }
};
#include <array>
class TriStateButton : public Button {
public:
    TriStateButton(GraphicsWindow::ShowConstraintMode*variable,
                   const std::array<GraphicsWindow::ShowConstraintMode,3> &states,
                   const std::array<std::string,3> &tooltips,
                   const std::array<std::string,3> &iconNames  ):variable(variable),states(states),tooltips(tooltips),iconNames(iconNames){}

    GraphicsWindow::ShowConstraintMode* variable;
    std::array<GraphicsWindow::ShowConstraintMode,3> states;
    std::array<std::string,3> tooltips;
    std::array<std::string,3> iconNames;
    std::shared_ptr<Pixmap> icons[3];

    std::string Tooltip() override {
        for (size_t k = 0; k < 3; ++k )
            if ( *variable == states[k] ) return tooltips[k];
        ssassert(false, "Unexpected mode");
    }

    void Draw(UiCanvas *uiCanvas, int x, int y, bool asHovered) override {
        for (size_t k = 0; k < 3;++k)
            if(icons[k] == nullptr)
                icons[k] = LoadPng("icons/text-window/" + iconNames[k] + ".png");

        std::shared_ptr<Pixmap> icon;
        for (size_t k = 0; k < 3; ++k )
            if ( *variable == states[k] ){
                icon = icons[k];
                break;
            }

        uiCanvas->DrawPixmap(icon, x, y - 24);
        if(asHovered) {
            uiCanvas->DrawRect(x - 2, x + 26, y + 2, y - 26,
                               /*fillColor=*/{ 255, 255, 0, 75 },
                               /*outlineColor=*/{});
        }
    }


    int AdvanceWidth() override { return 32; }

    void Click() override {
        for (size_t k = 0;k < 3;++k)
            if ( *variable == states[k] ){
                *variable = states[(k+1)%3];
                break;
            }

        SS.GenerateAll();
        SS.GW.Invalidate();
        SS.ScheduleShowTW();
    }
};


class OccludedLinesButton : public Button {
public:
    std::shared_ptr<Pixmap> visibleIcon;
    std::shared_ptr<Pixmap> stippledIcon;
    std::shared_ptr<Pixmap> invisibleIcon;

    std::string Tooltip() override {
        switch(SS.GW.drawOccludedAs) {
            case GraphicsWindow::DrawOccludedAs::INVISIBLE:
                return "Stipple occluded lines";

            case GraphicsWindow::DrawOccludedAs::STIPPLED:
                return "Draw occluded lines";

            case GraphicsWindow::DrawOccludedAs::VISIBLE:
                return "Don't draw occluded lines";

            default: ssassert(false, "Unexpected mode");
        }
    }

    void Draw(UiCanvas *uiCanvas, int x, int y, bool asHovered) override {
        if(visibleIcon == NULL) {
            visibleIcon = LoadPng("icons/text-window/occluded-visible.png");
        }
        if(stippledIcon == NULL) {
            stippledIcon = LoadPng("icons/text-window/occluded-stippled.png");
        }
        if(invisibleIcon == NULL) {
            invisibleIcon = LoadPng("icons/text-window/occluded-invisible.png");
        }

        std::shared_ptr<Pixmap> icon;
        switch(SS.GW.drawOccludedAs) {
            case GraphicsWindow::DrawOccludedAs::INVISIBLE: icon = invisibleIcon; break;
            case GraphicsWindow::DrawOccludedAs::STIPPLED:  icon = stippledIcon;  break;
            case GraphicsWindow::DrawOccludedAs::VISIBLE:   icon = visibleIcon;   break;
        }

        uiCanvas->DrawPixmap(icon, x, y - 24);
        if(asHovered) {
            uiCanvas->DrawRect(x - 2, x + 26, y + 2, y - 26,
                               /*fillColor=*/{ 255, 255, 0, 75 },
                               /*outlineColor=*/{});
        }
    }

    int AdvanceWidth() override { return 32; }

    void Click() override {
        switch(SS.GW.drawOccludedAs) {
            case GraphicsWindow::DrawOccludedAs::INVISIBLE:
                SS.GW.drawOccludedAs = GraphicsWindow::DrawOccludedAs::STIPPLED;
                break;

            case GraphicsWindow::DrawOccludedAs::STIPPLED:
                SS.GW.drawOccludedAs = GraphicsWindow::DrawOccludedAs::VISIBLE;
                break;

            case GraphicsWindow::DrawOccludedAs::VISIBLE:
                SS.GW.drawOccludedAs = GraphicsWindow::DrawOccludedAs::INVISIBLE;
                break;
        }

        SS.GenerateAll();
        SS.GW.Invalidate();
        SS.ScheduleShowTW();
    }
};

static SpacerButton   spacerButton;

static ShowHideButton workplanesButton =
    { &(SS.GW.showWorkplanes),   "workplane",     "workplanes from inactive groups" };
static ShowHideButton normalsButton =
    { &(SS.GW.showNormals),      "normal",        "normals"                         };
static ShowHideButton pointsButton =
    { &(SS.GW.showPoints),       "point",         "points"                          };
static ShowHideButton constructionButton =
    { &(SS.GW.showConstruction), "construction",  "construction entities"           };

/*static ShowHideButton constraintsButton =
    { &(SS.GW.showConstraints),  "constraint",    "constraints and dimensions"      };
*/
static TriStateButton constraintsButton =
    { &(SS.GW.showConstraints),
      {GraphicsWindow::ShowConstraintMode::SCM_SHOW_ALL,
       GraphicsWindow::ShowConstraintMode::SCM_SHOW_DIM,
       GraphicsWindow::ShowConstraintMode::SCM_NOSHOW},
      {"constraints and dimensions","dimensions","none"},
      {"constraint","constraint-dimo","constraint-wo"}
    };

static FacesButton facesButton;
static ShowHideButton shadedButton =
    { &(SS.GW.showShaded),       "shaded",        "shaded view of solid model"      };
static ShowHideButton edgesButton =
    { &(SS.GW.showEdges),        "edges",         "edges of solid model"            };
static ShowHideButton outlinesButton =
    { &(SS.GW.showOutlines),     "outlines",      "outline of solid model"          };
static ShowHideButton meshButton =
    { &(SS.GW.showMesh),         "mesh",          "triangle mesh of solid model"    };
static OccludedLinesButton occludedLinesButton;

static Button *buttons[] = {
    &workplanesButton,
    &normalsButton,
    &pointsButton,
    &constructionButton,
    &constraintsButton,
    &facesButton,
    &spacerButton,
    &shadedButton,
    &edgesButton,
    &outlinesButton,
    &meshButton,
    &spacerButton,
    &occludedLinesButton,
};

/** Foreground color codes. */
const TextWindow::Color TextWindow::fgColors[] = {
    { 'd', RGBi(255, 255, 255) },  // Default   : white
    { 'l', RGBi(100, 200, 255) },  // links     : blue
    { 't', RGBi(255, 200, 100) },  // tree/text : yellow
    { 'h', RGBi( 90,  90,  90) },
    { 's', RGBi( 40, 255,  40) },  // Ok        : green
    { 'm', RGBi(200, 200,   0) },
    { 'r', RGBi(  0,   0,   0) },  // Reverse   : black
    { 'x', RGBi(255,  20,  20) },  // Error     : red
    { 'i', RGBi(  0, 255, 255) },  // Info      : cyan
    { 'g', RGBi(160, 160, 160) },
    { 'b', RGBi(200, 200, 200) },
    { 0,   RGBi(  0,   0,   0) }
};
/** Background color codes. */
const TextWindow::Color TextWindow::bgColors[] = {
    { 'd', RGBi(  0,   0,   0) },  // Default   : black
    { 't', RGBi( 34,  15,  15) },
    { 'a', RGBi( 25,  25,  25) },  // Alternate : dark gray
    { 'r', RGBi(255, 255, 255) },  // Reverse   : white
    { 0,   RGBi(  0,   0,   0) }
};

void TextWindow::MakeColorTable(const Color *in, float *out) {
    int i;
    for(i = 0; in[i].c != 0; i++) {
        int c = in[i].c;
        ssassert(c >= 0 && c <= 255, "Unexpected color index");
        out[c*3 + 0] = in[i].color.redF();
        out[c*3 + 1] = in[i].color.greenF();
        out[c*3 + 2] = in[i].color.blueF();
    }
}

void TextWindow::Init() {
    if(!window) {
        window = Platform::CreateWindow(Platform::Window::Kind::TOOL, SS.GW.window);
        if(window) {
            canvas = CreateRenderer();

            using namespace std::placeholders;
            window->onClose = []() {
                SS.GW.showTextWindow = false;
                SS.GW.EnsureValidActives();
            };
            window->onMouseEvent = [this](Platform::MouseEvent event) {
                using Platform::MouseEvent;

                if(event.type == MouseEvent::Type::PRESS ||
                   event.type == MouseEvent::Type::DBL_PRESS ||
                   event.type == MouseEvent::Type::MOTION) {
                    bool isClick  = (event.type != MouseEvent::Type::MOTION);
                    bool leftDown = (event.button == MouseEvent::Button::LEFT);
                    this->MouseEvent(isClick, leftDown, event.x, event.y);
                    return true;
                } else if(event.type == MouseEvent::Type::LEAVE) {
                    MouseLeave();
                    return true;
                } else if(event.type == MouseEvent::Type::SCROLL_VERT) {
                    window->SetScrollbarPosition(window->GetScrollbarPosition() -
                                                 LINE_HEIGHT / 2 * event.scrollDelta);
                }
                return false;
            };
            window->onKeyboardEvent = SS.GW.window->onKeyboardEvent;
            window->onRender = std::bind(&TextWindow::Paint, this);
            window->onEditingDone = std::bind(&TextWindow::EditControlDone, this, _1);
            window->onScrollbarAdjusted = std::bind(&TextWindow::ScrollbarEvent, this, _1);
            window->SetMinContentSize(370, 370);
        }
    }

    ClearSuper();
}

void TextWindow::ClearSuper() {
    // Ugly hack, but not so ugly as the next line
    Platform::WindowRef oldWindow = std::move(window);
    std::shared_ptr<ViewportCanvas> oldCanvas = canvas;

    // Cannot use *this = {} here because TextWindow instances
    // are 2.4MB long; this causes stack overflows in prologue
    // when built with MSVC, even with optimizations.
    memset(this, 0, sizeof(*this));

    // Return old canvas
    window = std::move(oldWindow);
    canvas = oldCanvas;

    HideEditControl();

    MakeColorTable(fgColors, fgColorTable);
    MakeColorTable(bgColors, bgColorTable);

    ClearScreen();
    Show();
}

void TextWindow::HideEditControl() {
    editControl.colorPicker.show = false;
    if(window) {
        window->HideEditor();
        window->Invalidate();
    }
}

void TextWindow::ShowEditControl(int col, const std::string &str, int halfRow) {
    if(halfRow < 0) halfRow = top[hoveredRow];
    editControl.halfRow = halfRow;
    editControl.col = col;

    int x = LEFT_MARGIN + CHAR_WIDTH_*col;
    int y = (halfRow - SS.TW.scrollPos)*(LINE_HEIGHT/2);

    double width, height;
    window->GetContentSize(&width, &height);
    window->ShowEditor(x, y + LINE_HEIGHT - 2, LINE_HEIGHT - 4,
                       width - x, /*isMonospace=*/true, str);
}

void TextWindow::ShowEditControlWithColorPicker(int col, RgbaColor rgb) {
    SS.ScheduleShowTW();

    editControl.colorPicker.show = true;
    editControl.colorPicker.rgb = rgb;
    editControl.colorPicker.h = 0;
    editControl.colorPicker.s = 0;
    editControl.colorPicker.v = 1;
    ShowEditControl(col, ssprintf("%.2f, %.2f, %.2f", rgb.redF(), rgb.greenF(), rgb.blueF()));
}

void TextWindow::ClearScreen() {
    int i, j;
    for(i = 0; i < MAX_ROWS; i++) {
        for(j = 0; j < MAX_COLS; j++) {
            text[i][j] = ' ';
            meta[i][j].fg = 'd';
            meta[i][j].bg = 'd';
            meta[i][j].link = NOT_A_LINK;
        }
        top[i] = i*2;
    }
    rows = 0;
}

void TextWindow::Printf(bool halfLine, const char *fmt, ...) {
    if(!canvas) return;

    if(rows >= MAX_ROWS) return;

    va_list vl;
    va_start(vl, fmt);

    int r, c;
    r = rows;
    top[r] = (r == 0) ? 0 : (top[r-1] + (halfLine ? 3 : 2));
    rows++;

    for(c = 0; c < MAX_COLS; c++) {
        text[r][c] = ' ';
        meta[r][c].link = NOT_A_LINK;
    }

    char fg = 'd';
    char bg = 'd';
    RgbaColor bgRgb = RGBi(0, 0, 0);
    int link = NOT_A_LINK;
    uint32_t data = 0;
    LinkFunction *f = NULL, *h = NULL;

    c = 0;
    while(*fmt) {
        char buf[1024];

        if(*fmt == '%') {
            fmt++;
            if(*fmt == '\0') goto done;
            strcpy(buf, "");
            switch(*fmt) {
                case 'd': {
                    int v = va_arg(vl, int);
                    sprintf(buf, "%d", v);
                    break;
                }
                case 'x': {
                    unsigned int v = va_arg(vl, unsigned int);
                    sprintf(buf, "%08x", v);
                    break;
                }
                case '@': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.2f", v);
                    break;
                }
                case '2': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.2f", v < 0 ? "" : " ", v);
                    break;
                }
                case '3': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.3f", v < 0 ? "" : " ", v);
                    break;
                }
                case '#': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.3f", v);
                    break;
                }
                case 's': {
                    char *s = va_arg(vl, char *);
                    memcpy(buf, s, min(sizeof(buf), strlen(s)+1));
                    break;
                }
                case 'c': {
                    // 'char' is promoted to 'int' when passed through '...'
                    int v = va_arg(vl, int);
                    if(v == 0) {
                        strcpy(buf, "");
                    } else {
                        sprintf(buf, "%c", v);
                    }
                    break;
                }
                case 'E':
                    fg = 'd';
                    // leave the background, though
                    link = NOT_A_LINK;
                    data = 0;
                    f = NULL;
                    h = NULL;
                    break;

                case 'F':
                case 'B': {
                    char cc = fmt[1];  // color code
                    RgbaColor *rgbPtr = NULL;
                    switch(cc) {
                        case 0:   goto done;  // truncated directive
                        case 'p': cc = (char)va_arg(vl, int); break;
                        case 'z': rgbPtr = va_arg(vl, RgbaColor *); break;
                    }
                    if(*fmt == 'F') {
                        fg = cc;
                    } else {
                        bg = cc;
                        if(rgbPtr) bgRgb = *rgbPtr;
                    }
                    fmt++;
                    break;
                }
                case 'L':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    if(*fmt == 'p') {
                        link = va_arg(vl, int);
                    } else {
                        link = *fmt;
                    }
                    break;

                case 'f':
                    f = va_arg(vl, LinkFunction *);
                    break;

                case 'h':
                    h = va_arg(vl, LinkFunction *);
                    break;

                case 'D': {
                    unsigned int v = va_arg(vl, unsigned int);
                    data = (uint32_t)v;
                    break;
                }
                case '%':
                    strcpy(buf, "%");
                    break;
            }
        } else {
            utf8_iterator it2(fmt), it1 = it2++;
            strncpy(buf, fmt, it2 - it1);
            buf[it2 - it1] = '\0';
        }

        for(utf8_iterator it(buf); *it; ++it) {
            for(size_t i = 0; i < canvas->GetBitmapFont()->GetWidth(*it); i++) {
                if(c >= MAX_COLS) goto done;
                text[r][c] = (i == 0) ? *it : ' ';
                meta[r][c].fg = fg;
                meta[r][c].bg = bg;
                meta[r][c].bgRgb = bgRgb;
                meta[r][c].link = link;
                meta[r][c].data = data;
                meta[r][c].f = f;
                meta[r][c].h = h;
                c++;
            }
        }

        utf8_iterator it(fmt);
        it++;
        fmt = it.ptr();
    }
    while(c < MAX_COLS) {
        meta[r][c].fg = fg;
        meta[r][c].bg = bg;
        meta[r][c].bgRgb = bgRgb;
        c++;
    }

done:
    va_end(vl);
}

void TextWindow::Show() {
    if(SS.GW.pending.operation == GraphicsWindow::Pending::NONE) {
        SS.GW.ClearPending(/*scheduleShowTW=*/false);
    }

    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;

    // Make sure these tests agree with test used to draw indicator line on
    // main list of groups screen.
    if(SS.GW.pending.description) {
        // A pending operation (that must be completed with the mouse in
        // the graphics window) will preempt our usual display.
        HideEditControl();
        ShowHeader(false);
        Printf(false, "");
        Printf(false, "%s", SS.GW.pending.description);
        Printf(true, "%Fl%f%Ll(cancel operation)%E",
            &TextWindow::ScreenUnselectAll);
    } else if((gs.n > 0 || gs.constraints > 0) &&
                                    shown.screen != Screen::PASTE_TRANSFORMED)
    {
        if(edit.meaning != Edit::TTF_TEXT) HideEditControl();
        ShowHeader(false);
        DescribeSelection();
    } else {
        if(edit.meaning == Edit::TTF_TEXT) HideEditControl();
        ShowHeader(true);
        switch(shown.screen) {
            default:
                shown.screen = Screen::LIST_OF_GROUPS;
                // fall through
            case Screen::LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case Screen::GROUP_INFO:         ShowGroupInfo();        break;
            case Screen::GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
            case Screen::CONFIGURATION:      ShowConfiguration();    break;
            case Screen::STEP_DIMENSION:     ShowStepDimension();    break;
            case Screen::LIST_OF_STYLES:     ShowListOfStyles();     break;
            case Screen::STYLE_INFO:         ShowStyleInfo();        break;
            case Screen::PASTE_TRANSFORMED:  ShowPasteTransformed(); break;
            case Screen::EDIT_VIEW:          ShowEditView();         break;
            case Screen::TANGENT_ARC:        ShowTangentArc();       break;
        }
    }
    Printf(false, "");

    // Make sure there's room for the color picker
    if(editControl.colorPicker.show) {
        int pickerHeight = 25;
        int halfRow = editControl.halfRow;
        if(top[rows-1] - halfRow < pickerHeight && rows < MAX_ROWS) {
            rows++;
            top[rows-1] = halfRow + pickerHeight;
        }
    }

    if(window) {
        double width, height;
        window->GetContentSize(&width, &height);

        halfRows = (int)height / (LINE_HEIGHT/2);

        int bottom = top[rows-1] + 2;
        scrollPos = min(scrollPos, bottom - halfRows);
        scrollPos = max(scrollPos, 0);

        window->ConfigureScrollbar(0, top[rows - 1] + 1, halfRows);
        window->SetScrollbarPosition(scrollPos);
        window->SetScrollbarVisible(top[rows - 1] + 1 > halfRows);
        window->Invalidate();
    }
}

void TextWindow::DrawOrHitTestIcons(UiCanvas *uiCanvas, TextWindow::DrawOrHitHow how,
                                    double mx, double my)
{
    double width, height;
    window->GetContentSize(&width, &height);

    int x = 20, y = 33 + LINE_HEIGHT;
    y -= scrollPos*(LINE_HEIGHT/2);

    if(how == PAINT) {
        int top = y - 28, bot = y + 4;
        uiCanvas->DrawRect(0, (int)width, top, bot,
                           /*fillColor=*/{ 30, 30, 30, 255 }, /*outlineColor=*/{});
    }

    Button *oldHovered = hoveredButton;
    if(how != PAINT) {
        hoveredButton = NULL;
    }

    double hoveredX, hoveredY;
    for(Button *button : buttons) {
        if(how == PAINT) {
            button->Draw(uiCanvas, x, y, (button == hoveredButton));
        } else if(mx > x - 2 && mx < x + 26 &&
                  my < y + 2 && my > y - 26) {
            hoveredButton = button;
            hoveredX = x - 2;
            hoveredY = y - 26;
            if(how == CLICK) {
                button->Click();
            }
        }

        x += button->AdvanceWidth();
    }

    if(how != PAINT && hoveredButton != oldHovered) {
        if(hoveredButton == NULL) {
            window->SetTooltip("", 0, 0, 0, 0);
        } else {
            window->SetTooltip(hoveredButton->Tooltip(), hoveredX, hoveredY, 28, 28);
        }
        window->Invalidate();
    }
}

//----------------------------------------------------------------------------
// Given (x, y, z) = (h, s, v) in [0,6), [0,1], [0,1], return (x, y, z) =
// (r, g, b) all in [0, 1].
//----------------------------------------------------------------------------
Vector TextWindow::HsvToRgb(Vector hsv) {
    if(hsv.x >= 6) hsv.x -= 6;

    Vector rgb;
    double hmod2 = hsv.x;
    while(hmod2 >= 2) hmod2 -= 2;
    double x = (1 - fabs(hmod2 - 1));
    if(hsv.x < 1) {
        rgb = Vector::From(1, x, 0);
    } else if(hsv.x < 2) {
        rgb = Vector::From(x, 1, 0);
    } else if(hsv.x < 3) {
        rgb = Vector::From(0, 1, x);
    } else if(hsv.x < 4) {
        rgb = Vector::From(0, x, 1);
    } else if(hsv.x < 5) {
        rgb = Vector::From(x, 0, 1);
    } else {
        rgb = Vector::From(1, 0, x);
    }
    double c = hsv.y*hsv.z;
    double m = 1 - hsv.z;
    rgb = rgb.ScaledBy(c);
    rgb = rgb.Plus(Vector::From(m, m, m));

    return rgb;
}

std::shared_ptr<Pixmap> TextWindow::HsvPattern2d(int w, int h) {
    std::shared_ptr<Pixmap> pixmap = Pixmap::Create(Pixmap::Format::RGB, w, h);
    for(size_t j = 0; j < pixmap->height; j++) {
        size_t p = pixmap->stride * j;
        for(size_t i = 0; i < pixmap->width; i++) {
            Vector hsv = Vector::From(6.0*i/(pixmap->width-1), 1.0*j/(pixmap->height-1), 1);
            Vector rgb = HsvToRgb(hsv);
            rgb = rgb.ScaledBy(255);
            pixmap->data[p++] = (uint8_t)rgb.x;
            pixmap->data[p++] = (uint8_t)rgb.y;
            pixmap->data[p++] = (uint8_t)rgb.z;
        }
    }
    return pixmap;
}

std::shared_ptr<Pixmap> TextWindow::HsvPattern1d(double hue, double sat, int w, int h) {
    std::shared_ptr<Pixmap> pixmap = Pixmap::Create(Pixmap::Format::RGB, w, h);
    for(size_t i = 0; i < pixmap->height; i++) {
        size_t p = i * pixmap->stride;
        for(size_t j = 0; j < pixmap->width; j++) {
            Vector hsv = Vector::From(6*hue, sat, 1.0*(pixmap->width - 1 - j)/pixmap->width);
            Vector rgb = HsvToRgb(hsv);
            rgb = rgb.ScaledBy(255);
            pixmap->data[p++] = (uint8_t)rgb.x;
            pixmap->data[p++] = (uint8_t)rgb.y;
            pixmap->data[p++] = (uint8_t)rgb.z;
        }
    }
    return pixmap;
}

void TextWindow::ColorPickerDone() {
    RgbaColor rgb = editControl.colorPicker.rgb;
    EditControlDone(ssprintf("%.2f, %.2f, %.3f", rgb.redF(), rgb.greenF(), rgb.blueF()));
}

bool TextWindow::DrawOrHitTestColorPicker(UiCanvas *uiCanvas, DrawOrHitHow how, bool leftDown,
                                          double x, double y)
{
    using Platform::Window;

    bool mousePointerAsHand = false;

    if(how == HOVER && !leftDown) {
        editControl.colorPicker.picker1dActive = false;
        editControl.colorPicker.picker2dActive = false;
    }

    if(!editControl.colorPicker.show) return false;
    if(how == CLICK || (how == HOVER && leftDown)) window->Invalidate();

    static const RgbaColor BaseColor[12] = {
        RGBi(255,   0,   0),
        RGBi(  0, 255,   0),
        RGBi(  0,   0, 255),

        RGBi(  0, 255, 255),
        RGBi(255,   0, 255),
        RGBi(255, 255,   0),

        RGBi(255, 127,   0),
        RGBi(255,   0, 127),
        RGBi(  0, 255, 127),
        RGBi(127, 255,   0),
        RGBi(127,   0, 255),
        RGBi(  0, 127, 255),
    };

    double width, height;
    window->GetContentSize(&width, &height);

    int px = LEFT_MARGIN + CHAR_WIDTH_*editControl.col;
    int py = (editControl.halfRow - SS.TW.scrollPos)*(LINE_HEIGHT/2);

    py += LINE_HEIGHT + 5;

    static const int WIDTH = 16, HEIGHT = 12;
    static const int PITCH = 18, SIZE = 15;

    px = min(px, (int)width - (WIDTH*PITCH + 40));

    int pxm = px + WIDTH*PITCH + 11,
        pym = py + HEIGHT*PITCH + 7;

    int bw = 6;
    if(how == PAINT) {
        uiCanvas->DrawRect(px, pxm+bw, py, pym+bw,
                           /*fillColor=*/{ 50, 50, 50, 255 },
                           /*outlineColor=*/{},
                           /*zIndex=*/1);
        uiCanvas->DrawRect(px+(bw/2), pxm+(bw/2), py+(bw/2), pym+(bw/2),
                           /*fillColor=*/{ 0, 0, 0, 255 },
                           /*outlineColor=*/{},
                           /*zIndex=*/1);
    } else {
        if(x < px || x > pxm+(bw/2) ||
           y < py || y > pym+(bw/2))
        {
            return false;
        }
    }
    px += (bw/2);
    py += (bw/2);

    int i, j;
    for(i = 0; i < WIDTH/2; i++) {
        for(j = 0; j < HEIGHT; j++) {
            Vector rgb;
            RgbaColor d;
            if(i == 0 && j < 8) {
                d = SS.modelColor[j];
                rgb = Vector::From(d.redF(), d.greenF(), d.blueF());
            } else if(i == 0) {
                double a = (j - 8.0)/3.0;
                rgb = Vector::From(a, a, a);
            } else {
                d = BaseColor[j];
                rgb = Vector::From(d.redF(), d.greenF(), d.blueF());
                if(i >= 2 && i <= 4) {
                    double a = (i == 2) ? 0.2 : (i == 3) ? 0.3 : 0.4;
                    rgb = rgb.Plus(Vector::From(a, a, a));
                }
                if(i >= 5 && i <= 7) {
                    double a = (i == 5) ? 0.7 : (i == 6) ? 0.4 : 0.18;
                    rgb = rgb.ScaledBy(a);
                }
            }

            rgb = rgb.ClampWithin(0, 1);
            int sx = px + 5 + PITCH*(i + 8) + 4, sy = py + 5 + PITCH*j;

            if(how == PAINT) {
                uiCanvas->DrawRect(sx, sx+SIZE, sy, sy+SIZE,
                                   /*fillColor=*/RGBf(rgb.x, rgb.y, rgb.z),
                                   /*outlineColor=*/{},
                                   /*zIndex=*/2);
            } else if(how == CLICK) {
                if(x >= sx && x <= sx+SIZE && y >= sy && y <= sy+SIZE) {
                    editControl.colorPicker.rgb = RGBf(rgb.x, rgb.y, rgb.z);
                    ColorPickerDone();
                }
            } else if(how == HOVER) {
                if(x >= sx && x <= sx+SIZE && y >= sy && y <= sy+SIZE) {
                    mousePointerAsHand = true;
                }
            }
        }
    }

    int hxm, hym;
    int hx = px + 5, hy = py + 5;
    hxm = hx + PITCH*7 + SIZE;
    hym = hy + PITCH*2 + SIZE;
    if(how == PAINT) {
        uiCanvas->DrawRect(hx, hxm, hy, hym,
                           /*fillColor=*/editControl.colorPicker.rgb,
                           /*outlineColor=*/{},
                           /*zIndex=*/2);
    } else if(how == CLICK) {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            ColorPickerDone();
        }
    } else if(how == HOVER) {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            mousePointerAsHand = true;
        }
    }

    hy += PITCH*3;

    hxm = hx + PITCH*7 + SIZE;
    hym = hy + PITCH*1 + SIZE;
    // The one-dimensional thing to pick the color's value
    if(how == PAINT) {
        uiCanvas->DrawPixmap(HsvPattern1d(editControl.colorPicker.h,
                                          editControl.colorPicker.s,
                                          hxm-hx, hym-hy),
                             hx, hy, /*zIndex=*/2);

        int cx = hx+(int)((hxm-hx)*(1.0 - editControl.colorPicker.v));
        uiCanvas->DrawLine(cx, hy, cx, hym,
                           /*fillColor=*/{ 0, 0, 0, 255 },
                           /*outlineColor=*/{},
                           /*zIndex=*/3);
    } else if(how == CLICK ||
          (how == HOVER && leftDown && editControl.colorPicker.picker1dActive))
    {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            editControl.colorPicker.v = 1 - (x - hx)/(hxm - hx);

            Vector rgb = HsvToRgb(Vector::From(
                            6*editControl.colorPicker.h,
                            editControl.colorPicker.s,
                            editControl.colorPicker.v));
            editControl.colorPicker.rgb = RGBf(rgb.x, rgb.y, rgb.z);

            editControl.colorPicker.picker1dActive = true;
        }
    }
    // and advance our vertical position
    hy += PITCH*2;

    hxm = hx + PITCH*7 + SIZE;
    hym = hy + PITCH*6 + SIZE;
    // Two-dimensional thing to pick a color by hue and saturation
    if(how == PAINT) {
        uiCanvas->DrawPixmap(HsvPattern2d(hxm-hx, hym-hy), hx, hy,
                             /*zIndex=*/2);

        int cx = hx+(int)((hxm-hx)*editControl.colorPicker.h),
            cy = hy+(int)((hym-hy)*editControl.colorPicker.s);
        uiCanvas->DrawLine(cx - 5, cy, cx + 5, cy,
                           /*fillColor=*/{ 255, 255, 255, 255 },
                           /*outlineColor=*/{},
                           /*zIndex=*/3);
        uiCanvas->DrawLine(cx, cy - 5, cx, cy + 5,
                           /*fillColor=*/{ 255, 255, 255, 255 },
                           /*outlineColor=*/{},
                           /*zIndex=*/3);
    } else if(how == CLICK ||
          (how == HOVER && leftDown && editControl.colorPicker.picker2dActive))
    {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            double h = (x - hx)/(hxm - hx),
                   s = (y - hy)/(hym - hy);
            editControl.colorPicker.h = h;
            editControl.colorPicker.s = s;

            Vector rgb = HsvToRgb(Vector::From(
                            6*editControl.colorPicker.h,
                            editControl.colorPicker.s,
                            editControl.colorPicker.v));
            editControl.colorPicker.rgb = RGBf(rgb.x, rgb.y, rgb.z);

            editControl.colorPicker.picker2dActive = true;
        }
    }

    window->SetCursor(mousePointerAsHand ?
                      Window::Cursor::HAND :
                      Window::Cursor::POINTER);
    return true;
}

void TextWindow::Paint() {
    if (!canvas) return;

    double width, height;
    window->GetContentSize(&width, &height);

    Camera camera = {};
    camera.width      = width;
    camera.height     = height;
    camera.pixelRatio = window->GetDevicePixelRatio();
    camera.gridFit    = (window->GetDevicePixelRatio() == 1);
    camera.LoadIdentity();
    camera.offset.x   = -camera.width  / 2.0;
    camera.offset.y   = -camera.height / 2.0;

    Lighting lighting = {};
    lighting.backgroundColor = RGBi(0, 0, 0);

    canvas->SetLighting(lighting);
    canvas->SetCamera(camera);
    canvas->NewFrame();

    UiCanvas uiCanvas = {};
    uiCanvas.canvas = canvas;
    uiCanvas.flip = true;

    int r, c, a;
    for(a = 0; a < 2; a++) {
        for(r = 0; r < rows; r++) {
            int ltop = top[r];
            if(ltop < (scrollPos-1)) continue;
            if(ltop > scrollPos+halfRows) break;

            for(c = 0; c < min(((int)width/CHAR_WIDTH_)+1, (int) MAX_COLS); c++) {
                int x = LEFT_MARGIN + c*CHAR_WIDTH_;
                int y = (ltop-scrollPos)*(LINE_HEIGHT/2) + 4;

                int fg = meta[r][c].fg;
                int bg = meta[r][c].bg;

                // On the first pass, all the background quads; on the next
                // pass, all the foreground (i.e., font) quads.
                if(a == 0) {
                    RgbaColor bgRgb = meta[r][c].bgRgb;
                    int bh = LINE_HEIGHT, adj = 0;
                    if(bg == 'z') {
                        bh = CHAR_HEIGHT;
                        adj += 2;
                    } else {
                        bgRgb = RgbaColor::FromFloat(bgColorTable[bg*3+0],
                                                     bgColorTable[bg*3+1],
                                                     bgColorTable[bg*3+2]);
                    }

                    if(bg != 'd') {
                        // Move the quad down a bit, so that the descenders
                        // still have the correct background.
                        uiCanvas.DrawRect(x, x + CHAR_WIDTH_, y + adj, y + adj + bh,
                                          /*fillColor=*/bgRgb, /*outlineColor=*/{});
                    }
                } else if(a == 1) {
                    RgbaColor fgRgb = RgbaColor::FromFloat(fgColorTable[fg*3+0],
                                                           fgColorTable[fg*3+1],
                                                           fgColorTable[fg*3+2]);
                    if(text[r][c] != ' ') {
                        uiCanvas.DrawBitmapChar(text[r][c], x, y + CHAR_HEIGHT, fgRgb);
                    }

                    // If this is a link and it's hovered, then draw the
                    // underline
                    if(meta[r][c].link && meta[r][c].link != 'n' &&
                        (r == hoveredRow && c == hoveredCol))
                    {
                        int cs = c, cf = c;
                        while(cs >= 0 && meta[r][cs].link &&
                                         meta[r][cs].f    == meta[r][c].f &&
                                         meta[r][cs].data == meta[r][c].data)
                        {
                            cs--;
                        }
                        cs++;

                        while(          meta[r][cf].link &&
                                        meta[r][cf].f    == meta[r][c].f &&
                                        meta[r][cf].data == meta[r][c].data)
                        {
                            cf++;
                        }

                        // But don't underline checkboxes or radio buttons
                        while(((text[r][cs] >= 0xe000 && text[r][cs] <= 0xefff) ||
                                text[r][cs] == ' ') &&
                              cs < cf)
                        {
                            cs++;
                        }

                        // Always use the color of the rightmost character
                        // in the link, so that underline is consistent color
                        fg = meta[r][cf-1].fg;
                        fgRgb = RgbaColor::FromFloat(fgColorTable[fg*3+0],
                                                     fgColorTable[fg*3+1],
                                                     fgColorTable[fg*3+2]);
                        int yp = y + CHAR_HEIGHT;
                        uiCanvas.DrawLine(LEFT_MARGIN + cs*CHAR_WIDTH_, yp,
                                          LEFT_MARGIN + cf*CHAR_WIDTH_, yp,
                                          fgRgb);
                    }
                }
            }
        }
    }

    // The line to indicate the column of radio buttons that indicates the
    // active group.
    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;
    // Make sure this test agrees with test to determine which screen is drawn
    if(!SS.GW.pending.description && gs.n == 0 && gs.constraints == 0 &&
        shown.screen == Screen::LIST_OF_GROUPS)
    {
        int x = 29, y = 70 + LINE_HEIGHT;
        y -= scrollPos*(LINE_HEIGHT/2);

        RgbaColor color = RgbaColor::FromFloat(fgColorTable['t'*3+0],
                                               fgColorTable['t'*3+1],
                                               fgColorTable['t'*3+2]);
        uiCanvas.DrawLine(x, y, x, y+40, color);
    }

    // The header has some icons that are drawn separately from the text
    DrawOrHitTestIcons(&uiCanvas, PAINT, 0, 0);

    // And we may show a color picker for certain editable fields
    DrawOrHitTestColorPicker(&uiCanvas, PAINT, false, 0, 0);

    canvas->FlushFrame();
    canvas->Clear();
}

void TextWindow::MouseEvent(bool leftClick, bool leftDown, double x, double y) {
    using Platform::Window;

    if(SS.TW.window->IsEditorVisible() || SS.GW.window->IsEditorVisible()) {
        if(DrawOrHitTestColorPicker(NULL, leftClick ? CLICK : HOVER, leftDown, x, y)) {
            return;
        }

        if(leftClick) {
            HideEditControl();
            SS.GW.window->HideEditor();
        } else {
            window->SetCursor(Window::Cursor::POINTER);
        }
        return;
    }

    DrawOrHitTestIcons(NULL, leftClick ? CLICK : HOVER, x, y);

    GraphicsWindow::Selection ps = SS.GW.hover;
    SS.GW.hover.Clear();

    int prevHoveredRow = hoveredRow,
        prevHoveredCol = hoveredCol;
    hoveredRow = 0;
    hoveredCol = 0;

    // Find the corresponding character in the text buffer
    int c = (int)((x - LEFT_MARGIN) / CHAR_WIDTH_);
    int hh = (LINE_HEIGHT)/2;
    y += scrollPos*hh;
    int r;
    for(r = 0; r < rows; r++) {
        if(y >= top[r]*hh && y <= (top[r]+2)*hh) {
            break;
        }
    }
    if(r >= 0 && c >= 0 && r < rows && c < MAX_COLS) {
        window->SetCursor(Window::Cursor::POINTER);

        hoveredRow = r;
        hoveredCol = c;

        const auto &item = meta[r][c];
        if(leftClick) {
            if(item.link && item.f) {
                (item.f)(item.link, item.data);
                Show();
                SS.GW.Invalidate();
            }
        } else {
            if(item.link) {
                window->SetCursor(Window::Cursor::HAND);
                if(item.h) {
                    (item.h)(item.link, item.data);
                }
            } else {
                window->SetCursor(Window::Cursor::POINTER);
            }
        }
    }

    if((!ps.Equals(&(SS.GW.hover))) ||
        prevHoveredRow != hoveredRow ||
        prevHoveredCol != hoveredCol)
    {
        SS.GW.Invalidate();
        window->Invalidate();
    }
}

void TextWindow::MouseLeave() {
    hoveredButton = NULL;
    hoveredRow = 0;
    hoveredCol = 0;
    window->Invalidate();
}

void TextWindow::ScrollbarEvent(double newPos) {
    if(window->IsEditorVisible())
        return;

    int bottom = top[rows-1] + 2;
    newPos = min((int)newPos, bottom - halfRows);
    newPos = max((int)newPos, 0);

    if(newPos != scrollPos) {
        scrollPos = (int)newPos;
        window->Invalidate();
    }
}

}
