#pragma once

#include <Arduino.h>

/* **********************************************************************************
    The demo the boot splash's DEMO button starts: every panel in turn, each playing a short
    scripted scene of real 737-800 situations, by itself -- no Connector, no sim.

    A scene is a timeline of the messages MobiFlight would send, played through the panel's
    own set(), so the demo runs exactly the code the sim drives; plus presses of the panel's
    touch buttons, which show as a finger's would and send nothing (TouchZones' demo mode).

    The player is a pure function of time: advance() to any moment draws the same frame.
    That is how the host preview renders a scene for review (tools/preview/render_demo.py).

    The splash's own task runs it (Splash.cpp) and owns the screen while it does; nothing
    here knows about tasks.
********************************************************************************** */
namespace Demo
{
    enum Action : uint8_t {
        SET,     // send message `id` with payload `text`, through the panel's set()
        PRESS,   // press the touch zone named `text`
        RELEASE, // let go of it
    };

    struct Step {
        uint16_t    at; // ms from the start of the scene
        Action      action;
        int16_t     id;
        const char *text;
    };

    struct Scene {
        const char *type; // the panel, by its custom device type
        const Step *steps; // in time order
        uint8_t     count;
        uint16_t    length; // ms
    };

    uint8_t      sceneCount();
    const Scene &scene(uint8_t index);
    int8_t       findScene(const char *type); // -1 when there is none for that panel

    // The loop: start() brings up the first scene's panel, tick() plays it and moves on to
    // the next when it ends, round and round; stop() stops whichever panel is showing.
    void     start(uint32_t now);
    void     tick(uint32_t now);
    void     stop();
    uint32_t loops(); // full loops completed since start()

    // One scene on its own, for the host preview: begin() it, then advance() to any time.
    void begin(uint8_t index, uint32_t now);
    bool advance(uint32_t now); // false once the scene has run its length
} // namespace Demo
