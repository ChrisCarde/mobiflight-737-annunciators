#pragma once
/* The MobiFlight core's CmdMessenger, reduced to what the panel sources touch.

   The preview has no serial link and no Connector: button events from touch zones go
   nowhere. Panel state is driven directly by main.cpp instead. */

#include <Arduino.h>

enum { kButtonChange = 7, kDebug = 255 };

struct HostCmdMessenger {
    void sendCmdStart(int) {}
    void sendCmdArg(const char *) {}
    void sendCmdArg(int) {}
    void sendCmdEnd() {}
};

extern HostCmdMessenger cmdMessenger;
