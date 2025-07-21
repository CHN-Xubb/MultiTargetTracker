#include "DataStructures.h"


Measurement::Measurement(const Vector3& pos, double time, int obsId)
    : position(pos), timestamp(time), observerId(obsId) {}
