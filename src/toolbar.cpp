//-----------------------------------------------------------------------------
// The toolbar that appears at the top left of the graphics window, where the
// user can select icons with the mouse, to perform operations equivalent to
// selecting a menu item or using a keyboard shortcut.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

struct ToolIcon {
    std::string name;
    Command     command;
    const char *tooltip;
    std::shared_ptr<Pixmap> pixmap;
};
static ToolIcon Toolbar[] = {
    { "line",            Command::LINE_SEGMENT,
      N_("Sketch line segment"),                              {} },
    { "rectangle",       Command::RECTANGLE,
      N_("Sketch rectangle"),                                 {} },
    { "circle",          Command::CIRCLE,
      N_("Sketch circle"),                                    {} },
    { "arc",             Command::ARC,
      N_("Sketch arc of a circle"),                           {} },
    { "text",            Command::TTF_TEXT,
      N_("Sketch curves from text in a TrueType font"),       {} },
    { "image",           Command::IMAGE,
      N_("Sketch image from a file"),                         {} },
    { "tangent-arc",     Command::TANGENT_ARC,
      N_("Create tangent arc at selected point"),             {} },
    { "bezier",          Command::CUBIC,
      N_("Sketch cubic Bezier spline"),                       {} },
    { "point",           Command::DATUM_POINT,
      N_("Sketch datum point"),                               {} },
    { "construction",    Command::CONSTRUCTION,
      N_("Toggle construction"),                              {} },
    { "trim",            Command::SPLIT_CURVES,
      N_("Split lines / curves where they intersect"),        {} },
    { "",                Command::NONE, "",                   {} },

    { "length",          Command::DISTANCE_DIA,
      N_("Constrain distance / diameter / length"),           {} },
    { "angle",           Command::ANGLE,
      N_("Constrain angle"),                                  {} },
    { "horiz",           Command::HORIZONTAL,
      N_("Constrain to be horizontal"),                       {} },
    { "vert",            Command::VERTICAL,
      N_("Constrain to be vertical"),                         {} },
    { "parallel",        Command::PARALLEL,
      N_("Constrain to be parallel or tangent"),              {} },
    { "perpendicular",   Command::PERPENDICULAR,
      N_("Constrain to be perpendicular"),                    {} },
    { "pointonx",        Command::ON_ENTITY,
      N_("Constrain point on line / curve / plane / point"),  {} },
    { "symmetric",       Command::SYMMETRIC,
      N_("Constrain symmetric"),                              {} },
    { "equal",           Command::EQUAL,
      N_("Constrain equal length / radius / angle"),          {} },
    { "same-orientation",Command::ORIENTED_SAME,
      N_("Constrain normals in same orientation"),            {} },
    { "other-supp",      Command::OTHER_ANGLE,
      N_("Other supplementary angle"),                        {} },
    { "ref",             Command::REFERENCE,
      N_("Toggle reference dimension"),                       {} },

    { "ref",             Command::SELECT_ON,
      N_("Constrain Coincident Point by selection"),                       {} },

    { "",                Command::NONE, "",                   {} },

    { "extrude",         Command::GROUP_EXTRUDE,
      N_("New group extruding active sketch"),                {} },
    { "lathe",           Command::GROUP_LATHE,
      N_("New group rotating active sketch"),                 {} },
    { "step-rotate",     Command::GROUP_ROT,
      N_("New group step and repeat rotating"),               {} },
    { "step-translate",  Command::GROUP_TRANS,
      N_("New group step and repeat translating"),            {} },
    { "sketch-in-plane", Command::GROUP_WRKPL,
      N_("New group in new workplane (thru given entities)"), {} },
    { "sketch-in-3d",    Command::GROUP_3D,
      N_("New group in 3d"),                                  {} },
    { "assemble",        Command::GROUP_LINK,
      N_("New group linking / assembling file"),              {} },
    { "",                Command::NONE, "",                   {} },

    { "in3d",            Command::NEAREST_ISO,
      N_("Nearest isometric view"),                           {} },
    { "ontoworkplane",   Command::ONTO_WORKPLANE,
      N_("Align view to active workplane"),                   {} },
};

void GraphicsWindow::ToolbarDraw(UiCanvas *canvas) {
    ToolbarDrawOrHitTest(0, 0, canvas, NULL, NULL, NULL);
}

bool GraphicsWindow::ToolbarMouseMoved(int x, int y) {
    double width, height;
    window->GetContentSize(&width, &height);

    x += ((int)width/2);
    y += ((int)height/2);

    Command hitCommand;
    int hitX, hitY;
    bool withinToolbar = ToolbarDrawOrHitTest(x, y, NULL, &hitCommand, &hitX, &hitY);

    if(hitCommand != toolbarHovered) {
        toolbarHovered = hitCommand;
        Invalidate();
    }

    if(toolbarHovered != Command::NONE) {
        std::string tooltip;
        for(ToolIcon &icon : Toolbar) {
            if(toolbarHovered == icon.command) {
                tooltip = Translate(icon.tooltip);
            }
        }

        Platform::KeyboardEvent accel = SS.GW.AcceleratorForCommand(toolbarHovered);
        std::string accelDesc = Platform::AcceleratorDescription(accel);
        if(!accelDesc.empty()) {
            tooltip += ssprintf(" (%s)", accelDesc.c_str());
        }

        window->SetTooltip(tooltip, hitX, hitY, 32, 32);
    } else {
        window->SetTooltip("", 0, 0, 0, 0);
    }

    return withinToolbar;
}

bool GraphicsWindow::ToolbarMouseDown(int x, int y) {
    double width, height;
    window->GetContentSize(&width, &height);

    x += ((int)width/2);
    y += ((int)height/2);

    Command hitCommand;
    bool withinToolbar = ToolbarDrawOrHitTest(x, y, NULL, &hitCommand, NULL, NULL);
    if(hitCommand != Command::NONE) {
        SS.GW.ActivateCommand(hitCommand);
    }
    return withinToolbar;
}

bool GraphicsWindow::ToolbarDrawOrHitTest(int mx, int my, UiCanvas *canvas,
                                          Command *hitCommand, int *hitX, int *hitY)
{
    double width, height;
    window->GetContentSize(&width, &height);

    int x = 17, y = (int)(height - 52);

    // When changing these values, also change the asReference drawing code in drawentity.cpp.
    int fudge = 8;
    int h = 34*16 + 3*16 + fudge;
    int aleft = 0, aright = 66, atop = y+16+fudge/2, abot = y+16-h;

    bool withinToolbar =
        (mx >= aleft && mx <= aright && my <= atop && my >= abot);

    // Initialize/clear hitCommand.
    if(hitCommand) *hitCommand = Command::NONE;

    if(!canvas && !withinToolbar) {
        // This gets called every MouseMove event, so return quickly.
        return false;
    }

    if(canvas) {
        canvas->DrawRect(aleft, aright, atop, abot,
                        /*fillColor=*/{ 30, 30, 30, 255 },
                        /*outlineColor=*/{});
    }

    bool leftpos = true;
    for(ToolIcon &icon : Toolbar) {
        if(icon.name.empty()) { // spacer
            if(!leftpos) {
                leftpos = true;
                y -= 32;
                x -= 32;
            }
            y -= 16;

            if(canvas) {
                // Draw a separator bar in a slightly different color.
                int divw = 30, divh = 2;
                canvas->DrawRect(x+16+divw, x+16-divw, y+24+divh, y+24-divh,
                                 /*fillColor=*/{ 45, 45, 45, 255 },
                                 /*outlineColor=*/{});
            }

            continue;
        }

        if(icon.pixmap == nullptr) {
            icon.pixmap = LoadPng("icons/graphics-window/" + icon.name + ".png");
        }

        if(canvas) {
            canvas->DrawPixmap(icon.pixmap,
                               x - (int)icon.pixmap->width  / 2,
                               y - (int)icon.pixmap->height / 2);

            if(toolbarHovered == icon.command ||
               (pending.operation == Pending::COMMAND &&
                pending.command == icon.command)) {
                // Highlight the hovered or pending item.
                const int boxhw = 15;
                canvas->DrawRect(x+boxhw, x-boxhw, y+boxhw, y-boxhw,
                                 /*fillColor=*/{ 255, 255, 0, 75 },
                                 /*outlineColor=*/{});
            }
        } else {
            const int boxhw = 16;
            if(mx < (x+boxhw) && mx > (x - boxhw) &&
               my < (y+boxhw) && my > (y - boxhw))
            {
                if(hitCommand) *hitCommand = icon.command;
                if(hitX) *hitX = x - boxhw;
                if(hitY) *hitY = (int)height - (y + boxhw);
            }
        }

        if(leftpos) {
            x += 32;
            leftpos = false;
        } else {
            x -= 32;
            y -= 32;
            leftpos = true;
        }
    }

    return withinToolbar;
}
