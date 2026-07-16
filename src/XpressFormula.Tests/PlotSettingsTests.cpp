// PlotSettingsTests.cpp - Unit tests for UI plotting settings helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/PlotSettings.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI;

namespace XpressFormulaTests {

TEST_CASE(CoordinateOverlayPolicy_DisablesAxisTriadWhenCoordinatesAreOn) {
    bool showAxisTriad = true;

    const bool changed = resolveCoordinateOverlayPolicy(true, showAxisTriad);

    Assert::IsTrue(changed);
    Assert::IsFalse(showAxisTriad);
}

TEST_CASE(CoordinateOverlayPolicy_LeavesAxisTriadWhenCoordinatesAreOff) {
    bool showAxisTriad = true;

    const bool changed = resolveCoordinateOverlayPolicy(false, showAxisTriad);

    Assert::IsFalse(changed);
    Assert::IsTrue(showAxisTriad);
}

TEST_CASE(CoordinateOverlayPolicy_ReportsEffectiveAxisTriadVisibility) {
    Assert::IsFalse(isAxisTriadVisible(true, true));
    Assert::IsFalse(isAxisTriadVisible(false, false));
    Assert::IsTrue(isAxisTriadVisible(false, true));
}

TEST_CASE(PlotSettings_DefaultOverlayStateHasNoConflict) {
    PlotSettings settings;

    Assert::IsTrue(settings.showCoordinates);
    Assert::IsFalse(settings.showAxisTriad);
    Assert::IsFalse(settings.effectiveShowAxisTriad());
}

TEST_CASE(PlotSettings_ApplyCoordinateOverlayPolicyMutatesConflict) {
    PlotSettings settings;
    settings.showCoordinates = true;
    settings.showAxisTriad = true;

    const bool changed = settings.applyCoordinateOverlayPolicy();

    Assert::IsTrue(changed);
    Assert::IsFalse(settings.showAxisTriad);
    Assert::IsFalse(settings.effectiveShowAxisTriad());
}

} // namespace XpressFormulaTests
