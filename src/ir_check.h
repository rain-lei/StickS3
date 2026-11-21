#pragma once
#include "peripherals.h"

namespace rift {
// Diagnostic IR commands are only accepted in the nest.
bool pollIRCheck(Peripherals &device,bool nest);
bool watchEnabled(uint32_t now);
}
