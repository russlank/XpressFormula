// UiMetrics.cpp - Shared UI metric defaults.
#include "UiMetrics.h"

namespace XpressFormula::UI::UiKit {

const UiMetrics& metrics() {
    static const UiMetrics instance{};
    return instance;
}

} // namespace XpressFormula::UI::UiKit
