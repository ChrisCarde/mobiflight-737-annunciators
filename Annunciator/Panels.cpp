#include "Panels.h"
#include "MFCustomDevice.h"
#include "LampPanel.h"
#include "DoorPanel.h"
#include "IrsPanel.h"
#include "ElecPanel.h"
#include "McsPanel.h"
#include "AirPanel.h"
#include "FctlPanel.h"
#include "IsduPanel.h"
#include <string.h>

namespace Panels
{

// The panels built from a lamp table share the LampPanel engine for everything but init.
static const Ops PANELS[] = {
    {CUSTOMDEVICE_TYPE_DOOR, "DOOR ANNUNCIATOR", DoorPanel::init, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_IRS, "IRS ANNUNCIATOR", IrsPanel::init, IrsPanel::stop, IrsPanel::clear, IrsPanel::set, IrsPanel::update},
    {CUSTOMDEVICE_TYPE_ELEC, "ELECTRICAL METER", ElecPanel::init, ElecPanel::stop, ElecPanel::clear, ElecPanel::set, ElecPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_L, "MASTER CAUTION L", McsPanel::initLeft, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_R, "MASTER CAUTION R", McsPanel::initRight, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_BOTH, "MASTER CAUTION", McsPanel::initBoth, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_BOTH_AFDS, "CAUTION + AFDS", McsPanel::initBothAfds, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_AIR, "AIR CONDITIONING", AirPanel::init, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_FCTL, "FLIGHT CONTROLS", FctlPanel::init, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_ISDU, "IRS DISPLAY UNIT", IsduPanel::init, IsduPanel::stop, IsduPanel::clear, IsduPanel::set, IsduPanel::update},
};

uint8_t count() { return (uint8_t)(sizeof(PANELS) / sizeof(PANELS[0])); }

const Ops &at(uint8_t index) { return PANELS[index < count() ? index : 0]; }

int8_t find(const char *type)
{
    for (uint8_t i = 0; i < count(); ++i)
        if (type && strcmp(type, PANELS[i].type) == 0) return (int8_t)i;
    return -1;
}

} // namespace Panels
