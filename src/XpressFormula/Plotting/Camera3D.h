// Camera3D.h - Immutable 3D camera inputs for plot projection.
#pragma once

namespace XpressFormula::Plotting {

struct Camera3D {
    float azimuthDeg = 40.0f;
    float elevationDeg = 30.0f;
    float zScale = 1.0f;
};

} // namespace XpressFormula::Plotting
