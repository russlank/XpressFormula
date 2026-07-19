// ViewState.h - Persistent and transient view geometry models.
#pragma once

namespace XpressFormula::Model {

struct ViewState {
    double centerX = 0.0;
    double centerY = 0.0;
    double scaleX = 60.0;
    double scaleY = 60.0;
};

struct Viewport {
    float originX = 0.0f;
    float originY = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

} // namespace XpressFormula::Model
