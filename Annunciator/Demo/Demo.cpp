#include "Demo.h"
#include "Board.h"
#include "Panels.h"
#include "PanelGfx.h"
#include "TouchZones.h"
#include <string.h>
#if ANNUN_BACKEND_HOST
#include <cstdio>
#endif

namespace Demo
{

namespace
{
const Panels::Ops *s_ops   = nullptr; // the panel playing
uint8_t            s_scene = 0;
uint8_t            s_next  = 0; // its next step
uint32_t           s_t0    = 0; // when it started
uint32_t           s_loops = 0;

void apply(const Step &step)
{
    switch (step.action) {
    case SET: {
        char payload[16]; // set() takes a buffer it may write to; a scene's text is in flash
        strncpy(payload, step.text ? step.text : "", sizeof(payload) - 1);
        payload[sizeof(payload) - 1] = 0;
        s_ops->set(step.id, payload);
        break;
    }
    case PRESS:
        if (!TouchZones::pressScripted(step.text)) {
#if ANNUN_BACKEND_HOST
            fprintf(stderr, "demo: %s has no touch zone named \"%s\"\n", scene(s_scene).type, step.text);
#endif
        }
        break;
    case RELEASE:
        TouchZones::releaseScripted();
        break;
    }
}
} // namespace

void begin(uint8_t index, uint32_t now)
{
    stop(); // the last scene's panel, and dark while the next one draws
    s_scene = index < sceneCount() ? index : 0;
    const int8_t panel = Panels::find(scene(s_scene).type);
    if (panel < 0) return;
    s_ops  = &Panels::at((uint8_t)panel);
    s_ops->init(Board::BACKLIGHT_PIN);
    s_next = 0;
    s_t0   = now;
    advance(now); // the steps at 0 ms, and the first frame on the glass before it is lit
    PanelGfx::setShowing(true);
}

bool advance(uint32_t now)
{
    if (!s_ops) return false;
    const Scene   &sc = scene(s_scene);
    const uint32_t t  = now - s_t0;
    while (s_next < sc.count && sc.steps[s_next].at <= t) apply(sc.steps[s_next++]);
    s_ops->update();
    PanelGfx::present();
    return t < sc.length;
}

void start(uint32_t now)
{
    s_loops = 0;
    begin(0, now);
}

void tick(uint32_t now)
{
    if (advance(now)) return;
    const uint8_t next = (uint8_t)((s_scene + 1) % sceneCount());
    if (next == 0) ++s_loops;
    begin(next, now);
}

void stop()
{
    TouchZones::releaseScripted();
    if (s_ops) s_ops->stop();
    s_ops = nullptr;
    PanelGfx::setShowing(false);
}

uint32_t loops() { return s_loops; }

} // namespace Demo
