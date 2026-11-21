#pragma once
#include "peripherals.h"

namespace rift {
// Poll only in the nest. Returns true while the check owns the peripherals.
bool pollIRCheck(Peripherals &device);
}
