/* Host preview: renders a panel to a PPM using the firmware's own panel code.

   Usage:  preview <panel> <scene> <out.ppm>
           preview splash <on|nopanel> <out.ppm>

   panels  door irs elec mcs_l mcs_r mcs_both mcs_both_afds air fctl isdu
   scenes  off      nothing lit, the way the panel sits with the aircraft quiet
           demo     a representative mix of lit, unlit and blinking lamps
           all      every lamp on, as the lamp-test message drives it
           dim      the demo scene with night dimming applied
           press    the demo scene with a finger held on one of the buttons

   splash  the boot splash (on), and what a board with no panel shows after it (nopanel).
           A configured board shows its panel's off scene there instead.

   Both resolutions are the same binary built twice; see build.py. Layout, measurements and
   text all come from the firmware sources, so what appears here is what the board draws --
   to the pixel for positions and glyphs, and near enough for anti-aliased edges, which are
   this host's own. */

#include "Board.h"
#include "Panel.h"
#include "PanelGfx.h"
#include "LampPanel.h"
#include "DoorPanel.h"
#include "IrsPanel.h"
#include "ElecPanel.h"
#include "McsPanel.h"
#include "AirPanel.h"
#include "FctlPanel.h"
#include "IsduPanel.h"
#include "Splash.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{

struct PanelOps {
    const char *name;
    void (*init)(uint8_t);
    void (*clear)();
    void (*set)(int16_t, char *);
    void (*update)();
    uint8_t lampCount; // 0 for panels that are not a lamp table
};

// The same ten panels as MFCustomDevice.cpp's dispatch table, in the same order.
const PanelOps PANELS[] = {
    {"door", DoorPanel::init, LampPanel::clear, LampPanel::set, LampPanel::update, DoorPanel::LAMP_COUNT},
    {"irs", IrsPanel::init, IrsPanel::clear, IrsPanel::set, IrsPanel::update, 11},
    {"elec", ElecPanel::init, ElecPanel::clear, ElecPanel::set, ElecPanel::update, ElecPanel::LAMP_COUNT},
    {"mcs_l", McsPanel::initLeft, LampPanel::clear, LampPanel::set, LampPanel::update, 8},
    {"mcs_r", McsPanel::initRight, LampPanel::clear, LampPanel::set, LampPanel::update, 8},
    {"mcs_both", McsPanel::initBoth, LampPanel::clear, LampPanel::set, LampPanel::update, 14},
    {"mcs_both_afds", McsPanel::initBothAfds, LampPanel::clear, LampPanel::set, LampPanel::update, 17},
    {"air", AirPanel::init, LampPanel::clear, LampPanel::set, LampPanel::update, 16},
    {"fctl", FctlPanel::init, LampPanel::clear, LampPanel::set, LampPanel::update, 9},
    {"isdu", IsduPanel::init, IsduPanel::clear, IsduPanel::set, IsduPanel::update, 0},
};

void send(const PanelOps &p, int16_t id, const char *value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", value);
    p.set(id, buf);
}

// A lamp scene that exercises every style the renderer can draw: lit, unlit, and one
// blinking lamp caught in its bright phase.
void demoLamps(const PanelOps &p)
{
    for (uint8_t i = 0; i < p.lampCount; ++i) send(p, (int16_t)i, (i % 3 == 0) ? "1" : "0");
    if (p.lampCount > 1) send(p, 1, "2"); // blink
}

void demoPanel(const PanelOps &p)
{
    const std::string n = p.name;
    if (n == "isdu") {
        send(p, IsduPanel::MSG_LEFT, "N4732.4");
        send(p, IsduPanel::MSG_RIGHT, "W12212.3");
        send(p, IsduPanel::MSG_DOTS, "1");
        send(p, IsduPanel::MSG_DSPL_SEL, "2"); // PPOS
        send(p, IsduPanel::MSG_SYS_DSPL, "0"); // L
        send(p, IsduPanel::MSG_ENT_CUE, "1");
        send(p, IsduPanel::MSG_CLR_CUE, "0");
        return;
    }
    demoLamps(p);
    if (n == "elec") {
        send(p, ElecPanel::MSG_LINE_TOP, "  28   400  ");
        send(p, ElecPanel::MSG_LINE_BOTTOM, " 24  115 400");
        send(p, ElecPanel::MSG_DC_SELECTOR, "2");
        send(p, ElecPanel::MSG_AC_SELECTOR, "1");
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: %s <panel> <off|demo|all|dim|press> <out.ppm> [x,y]\n"
                        "  x,y places the finger for the press scene; it defaults to a\n"
                        "  button that exists on every panel.\n", argv[0]);
        return 2;
    }
    const std::string want = argv[1], scene = argv[2];

    if (want == "splash") {
        if (scene != "on" && scene != "nopanel") {
            fprintf(stderr, "the splash scenes are on and nopanel\n");
            return 2;
        }
        PanelGfx::begin(0);
        if (scene == "on") Splash::draw();
        else Splash::drawNoPanel();
        if (!PanelGfx::tft().writePpm(argv[3])) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 1;
        }
        printf("splash %s -> %s (%dx%d)\n", scene.c_str(), argv[3],
               (int)Board::PANEL_W, (int)Board::PANEL_H);
        return 0;
    }

    const PanelOps *p = nullptr;
    for (const PanelOps &c : PANELS)
        if (want == c.name) p = &c;
    if (!p) {
        fprintf(stderr, "unknown panel %s\n", want.c_str());
        return 2;
    }

    hostSetMillis(1000);
    p->init(0);
    p->clear();

    // A live panel: MobiFlight running, the annunciator bus powered, past the wake settle.
    PanelGfx::setRunning(true);
    PanelGfx::setBusPowered(true);
    hostSetMillis(2000);
    PanelGfx::tick();

    if (scene == "press") {
        demoPanel(*p);
        // Where the finger lands, as a fraction of the panel, so the same scene works at
        // either size: the outboard push-light on the glareshield panels, and a lamp in
        // the grid on the others.
        int16_t px = 0, py = 0;
        if (argc == 5 && sscanf(argv[4], "%hd,%hd", &px, &py) == 2) {
            // an explicit point, for checking a particular button
        } else {
            const bool  glareshield = want.rfind("mcs", 0) == 0;
            const float fx = glareshield ? 0.13f : 0.45f;
            const float fy = glareshield ? 0.50f : 0.30f;
            px = (int16_t)(Board::PANEL_W * fx);
            py = (int16_t)(Board::PANEL_H * fy);
        }
        hostSetTouch(px, py);
        p->update(); // the press is picked up here, and the lamp redrawn held down
    } else if (scene == "all") {
        send(*p, MSG_LAMP_TEST, "1");
    } else if (scene == "demo" || scene == "dim") {
        demoPanel(*p);
        if (scene == "dim") send(*p, MSG_DIM, "1");
    }

    p->update();
    if (!PanelGfx::tft().writePpm(argv[3])) {
        fprintf(stderr, "cannot write %s\n", argv[3]);
        return 1;
    }
    printf("%s %s -> %s (%dx%d)\n", want.c_str(), scene.c_str(), argv[3],
           (int)Board::PANEL_W, (int)Board::PANEL_H);
    return 0;
}
