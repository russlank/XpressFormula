// ViewTransformTests.cpp - Unit tests for the viewport transform.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/ViewTransform.h"
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::Core;

namespace XpressFormulaTests {

static void assertClose(double expected, double actual,
                        double tol = 1e-6) {
    Assert::IsTrue(std::abs(expected - actual) < tol,
        (std::to_wstring(expected) + L" != " + std::to_wstring(actual)).c_str());
}

static ViewTransform makeDefault() {
    ViewTransform vt;
    vt.screenWidth = 800;
    vt.screenHeight = 600;
    vt.screenOriginX = 0;
    vt.screenOriginY = 0;
    vt.centerX = 0;
    vt.centerY = 0;
    vt.scaleX = 100;
    vt.scaleY = 100;
    return vt;
}

TEST_CASE(WorldToScreen_Origin) {
    auto vt = makeDefault();
    Vec2 s = vt.worldToScreen(0, 0);
    // Origin should map to center of screen
    assertClose(400.0, s.x);
    assertClose(300.0, s.y);
}

TEST_CASE(WorldToScreen_PositiveX) {
    auto vt = makeDefault();
    Vec2 s = vt.worldToScreen(1, 0);
    // 1 world unit right = 100 pixels right of center
    assertClose(500.0, s.x);
    assertClose(300.0, s.y);
}

TEST_CASE(WorldToScreen_PositiveY) {
        auto vt = makeDefault();
        Vec2 s = vt.worldToScreen(0, 1);
        // 1 world unit up = 100 pixels up (lower screen Y)
        assertClose(400.0, s.x);
        assertClose(200.0, s.y);
}

    TEST_CASE(ScreenToWorld_Center) {
        auto vt = makeDefault();
        double wx, wy;
        vt.screenToWorld(400, 300, wx, wy);
        assertClose(0.0, wx);
        assertClose(0.0, wy);
    }

    TEST_CASE(ScreenToWorld_Roundtrip) {
        auto vt = makeDefault();
        double origX = 2.5, origY = -1.3;
        Vec2 s = vt.worldToScreen(origX, origY);
        double wx, wy;
        vt.screenToWorld(s.x, s.y, wx, wy);
        assertClose(origX, wx);
        assertClose(origY, wy);
    }

    TEST_CASE(ZoomAll_Increases) {
        auto vt = makeDefault();
        double oldScale = vt.scaleX;
        vt.zoomAll(2.0);
        Assert::IsTrue(vt.scaleX > oldScale);
        Assert::IsTrue(vt.scaleY > oldScale);
    }

    TEST_CASE(ZoomAll_Factor) {
        auto vt = makeDefault();
        vt.zoomAll(2.0);
        assertClose(200.0, vt.scaleX);
        assertClose(200.0, vt.scaleY);
    }

    TEST_CASE(ZoomX_OnlyAffectsX) {
        auto vt = makeDefault();
        double oldScaleY = vt.scaleY;
        vt.zoomX(2.0);
        assertClose(200.0, vt.scaleX);
        assertClose(oldScaleY, vt.scaleY);
    }

    TEST_CASE(ZoomY_OnlyAffectsY) {
        auto vt = makeDefault();
        double oldScaleX = vt.scaleX;
        vt.zoomY(2.0);
        assertClose(oldScaleX, vt.scaleX);
        assertClose(200.0, vt.scaleY);
    }

    TEST_CASE(Pan_ShiftsCenter) {
        auto vt = makeDefault();
        vt.pan(1.0, 2.0);
        assertClose(1.0, vt.centerX);
        assertClose(2.0, vt.centerY);
    }

    TEST_CASE(PanPixels_ShiftsCenter) {
        auto vt = makeDefault();
        // Panning 100 pixels right at scale 100 = -1 world unit
        vt.panPixels(100.0f, 0.0f);
        assertClose(-1.0, vt.centerX);
    }

    TEST_CASE(Reset_RestoresDefaults) {
        auto vt = makeDefault();
        vt.pan(5.0, 5.0);
        vt.zoomAll(10.0);
        vt.reset();
        assertClose(0.0, vt.centerX);
        assertClose(0.0, vt.centerY);
        assertClose(60.0, vt.scaleX); // DEFAULT_SCALE
    }

    TEST_CASE(WorldRange_Symmetric) {
        auto vt = makeDefault();
        assertClose(-vt.worldXMin(), vt.worldXMax());
        assertClose(-vt.worldYMin(), vt.worldYMax());
    }

    TEST_CASE(WorldRange_Width) {
        auto vt = makeDefault();
        double width = vt.worldXMax() - vt.worldXMin();
        // 800 pixels / 100 px/unit = 8 world units
        assertClose(8.0, width);
    }

    TEST_CASE(GridSpacing_Positive) {
        auto vt = makeDefault();
        Assert::IsTrue(vt.gridSpacingX() > 0);
        Assert::IsTrue(vt.gridSpacingY() > 0);
    }

    TEST_CASE(GridSpacing_NiceNumbers) {
        auto vt = makeDefault();
        double gs = vt.gridSpacingX();
        // Should be a "nice" number: 1, 2, or 5 times a power of 10
        double log10gs = std::log10(gs);
        double magnitude = std::pow(10.0, std::floor(log10gs));
        double norm = gs / magnitude;
        // norm should be close to 1, 2, or 5
        bool nice = (std::abs(norm - 1.0) < 0.01 ||
                     std::abs(norm - 2.0) < 0.01 ||
                     std::abs(norm - 5.0) < 0.01);
        Assert::IsTrue(nice, L"Grid spacing is not a nice number");
    }

TEST_CASE(WithOffset_OriginShifted) {
    auto vt = makeDefault();
    vt.screenOriginX = 100;
    vt.screenOriginY = 50;
    Vec2 s = vt.worldToScreen(0, 0);
    assertClose(500.0, s.x); // 100 + 800/2
    assertClose(350.0, s.y); // 50 + 600/2
}

// ----- Edge-case tests -----

TEST_CASE(ZoomAll_ClampMin) {
    auto vt = makeDefault();
    // Scale starts at 100. Zoom with very small factor to hit MIN_SCALE (0.1)
    vt.zoomAll(0.0001);
    assertClose(0.1, vt.scaleX);
    assertClose(0.1, vt.scaleY);
}

TEST_CASE(ZoomAll_ClampMax) {
    auto vt = makeDefault();
    // Scale starts at 100. Zoom with very large factor to hit MAX_SCALE (100000)
    vt.zoomAll(10000.0);
    assertClose(100000.0, vt.scaleX);
    assertClose(100000.0, vt.scaleY);
}

TEST_CASE(ZoomX_ClampMin) {
    auto vt = makeDefault();
    vt.zoomX(0.0001);
    assertClose(0.1, vt.scaleX);
    assertClose(100.0, vt.scaleY); // Y unchanged
}

TEST_CASE(ZoomY_ClampMax) {
    auto vt = makeDefault();
    vt.zoomY(10000.0);
    assertClose(100.0, vt.scaleX); // X unchanged
    assertClose(100000.0, vt.scaleY);
}

TEST_CASE(PanPixels_YDirection) {
    auto vt = makeDefault();
    // Panning 100 pixels down at scale 100 = +1 world unit (Y inverted)
    vt.panPixels(0.0f, 100.0f);
    assertClose(0.0, vt.centerX);
    assertClose(1.0, vt.centerY);
}

TEST_CASE(PanPixels_Diagonal) {
    auto vt = makeDefault();
    vt.panPixels(200.0f, 200.0f);
    assertClose(-2.0, vt.centerX); // 200/100 panned left
    assertClose(2.0, vt.centerY);  // 200/100 panned up
}

TEST_CASE(ScreenToWorld_TopLeft) {
    auto vt = makeDefault();
    double wx, wy;
    vt.screenToWorld(0, 0, wx, wy);
    // Top-left: (-400/100, 300/100) = (-4, 3)
    assertClose(-4.0, wx);
    assertClose(3.0, wy);
}

TEST_CASE(ScreenToWorld_BottomRight) {
    auto vt = makeDefault();
    double wx, wy;
    vt.screenToWorld(800, 600, wx, wy);
    assertClose(4.0, wx);
    assertClose(-3.0, wy);
}

TEST_CASE(NegativeCenter) {
    auto vt = makeDefault();
    vt.centerX = -5.0;
    vt.centerY = -3.0;
    Vec2 s = vt.worldToScreen(-5.0, -3.0);
    // Center of screen
    assertClose(400.0, s.x);
    assertClose(300.0, s.y);
}

TEST_CASE(AsymmetricScale) {
    auto vt = makeDefault();
    vt.scaleX = 50;
    vt.scaleY = 200;
    double wx, wy;
    // Roundtrip with asymmetric scale
    Vec2 s = vt.worldToScreen(2.0, 1.0);
    vt.screenToWorld(s.x, s.y, wx, wy);
    assertClose(2.0, wx);
    assertClose(1.0, wy);
}

TEST_CASE(WorldRange_AfterPan) {
    auto vt = makeDefault();
    vt.pan(3.0, 2.0);
    // Range should shift by the pan amount
    double halfWidth = 800.0 / (2.0 * 100.0); // 4.0
    assertClose(3.0 - halfWidth, vt.worldXMin());
    assertClose(3.0 + halfWidth, vt.worldXMax());
}

TEST_CASE(WorldRange_AsymmetricScale) {
    auto vt = makeDefault();
    vt.scaleX = 400; // more zoomed in on X
    double xRange = vt.worldXMax() - vt.worldXMin();
    double yRange = vt.worldYMax() - vt.worldYMin();
    // X range should be smaller than Y range
    Assert::IsTrue(xRange < yRange);
}

TEST_CASE(GridSpacing_HighZoom) {
    auto vt = makeDefault();
    vt.scaleX = 10000.0; // very zoomed in
    double gs = vt.gridSpacingX();
    Assert::IsTrue(gs > 0);
    Assert::IsTrue(gs < 0.1); // small grid spacing for high zoom
}

TEST_CASE(GridSpacing_LowZoom) {
    auto vt = makeDefault();
    vt.scaleX = 1.0; // very zoomed out
    double gs = vt.gridSpacingX();
    Assert::IsTrue(gs > 0);
    Assert::IsTrue(gs >= 50.0); // large grid spacing for low zoom
}

TEST_CASE(Pan_Cumulative) {
    auto vt = makeDefault();
    vt.pan(1.0, 0.0);
    vt.pan(2.0, 3.0);
    assertClose(3.0, vt.centerX);
    assertClose(3.0, vt.centerY);
}

TEST_CASE(ZoomAll_Multiple) {
    auto vt = makeDefault();
    vt.zoomAll(2.0);
    vt.zoomAll(0.5);
    // Should return to original scale
    assertClose(100.0, vt.scaleX);
    assertClose(100.0, vt.scaleY);
}

TEST_CASE(Reset_AfterPanAndZoom) {
    auto vt = makeDefault();
    vt.pan(10.0, -5.0);
    vt.zoomAll(50.0);
    vt.reset();
    assertClose(0.0, vt.centerX);
    assertClose(0.0, vt.centerY);
    assertClose(60.0, vt.scaleX); // DEFAULT_SCALE
    assertClose(60.0, vt.scaleY);
}

TEST_CASE(WorldToScreen_NegativeCoord) {
    auto vt = makeDefault();
    Vec2 s = vt.worldToScreen(-2.0, -1.0);
    // -2 world units left: 400 - 200 = 200
    assertClose(200.0, s.x);
    // -1 world unit down: 300 + 100 = 400
    assertClose(400.0, s.y);
}

} // namespace XpressFormulaTests

