#pragma once
#include <Eigen/Dense>
#include "nlohmann/json.hpp"


using Vector3 = Eigen::Vector3d;
using json = nlohmann::json;

class Measurement {
public:
    Vector3 position;
    double timestamp;
    int observerId;

    Measurement() = default;

    Measurement(const Vector3& pos, double time, int obsId);
};


