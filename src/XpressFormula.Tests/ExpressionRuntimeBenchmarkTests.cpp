#include "CppUnitTest.h"

#include "../XpressFormula/Core/Evaluator.h"
#include "../XpressFormula/Core/Parser.h"
#include "../XpressFormula/Plotting/Geometry/Bounds.h"

#include <chrono>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula;

namespace {

bool expressionBenchmarkEnabled() noexcept {
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "XF_RUN_EXPRESSION_BENCHMARK") != 0 || value == nullptr) {
        return false;
    }

    const bool enabled = std::string_view(value) != "0";
    std::free(value);
    return enabled;
}

Core::ASTNodePtr parseBenchmarkFormula(std::string_view text) {
    Core::Parser::Result parsed = Core::Parser::parse(std::string(text));
    Assert::IsTrue(parsed.success());
    return parsed.ast;
}

template <typename Func>
double elapsedMilliseconds(Func&& func) {
    const auto start = std::chrono::steady_clock::now();
    func();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

struct TimedSample {
    double milliseconds = 0.0;
    double checksum = 0.0;
};

void consumeSample(double value, double& checksum) noexcept {
    if (std::isfinite(value)) {
        checksum += std::abs(value) + 1.0;
    }
}

TimedSample measureCurveWithMap(const Core::ASTNodePtr& ast, int repetitions) {
    constexpr double xMin = -6.283185307179586;
    constexpr double xMax = 6.283185307179586;
    constexpr int sampleCount = 4096;
    const double dx = (xMax - xMin) / static_cast<double>(sampleCount);

    double checksum = 0.0;
    const double ms = elapsedMilliseconds([&]() {
        Core::Evaluator::Variables vars;
        for (int rep = 0; rep < repetitions; ++rep) {
            for (int i = 0; i <= sampleCount; ++i) {
                vars["x"] = xMin + static_cast<double>(i) * dx;
                consumeSample(Core::Evaluator::evaluate(ast, vars), checksum);
            }
        }
    });

    return { ms, checksum };
}

TimedSample measureCurveWithSlots(const Core::ASTNodePtr& ast, int repetitions) {
    constexpr double xMin = -6.283185307179586;
    constexpr double xMax = 6.283185307179586;
    constexpr int sampleCount = 4096;
    const double dx = (xMax - xMin) / static_cast<double>(sampleCount);

    double checksum = 0.0;
    const double ms = elapsedMilliseconds([&]() {
        Core::EvaluationContext context;
        for (int rep = 0; rep < repetitions; ++rep) {
            for (int i = 0; i <= sampleCount; ++i) {
                context.x = xMin + static_cast<double>(i) * dx;
                consumeSample(Core::Evaluator::evaluate(ast, context), checksum);
            }
        }
    });

    return { ms, checksum };
}

TimedSample measureExplicitSurfaceWithMap(const Core::ASTNodePtr& ast, int repetitions) {
    const Plotting::Geometry::Bounds2D bounds{ -6.0, 6.0, -6.0, 6.0 };
    constexpr int resolution = 96;
    const double dx = (bounds.xMax - bounds.xMin) / static_cast<double>(resolution);
    const double dy = (bounds.yMax - bounds.yMin) / static_cast<double>(resolution);

    double checksum = 0.0;
    const double ms = elapsedMilliseconds([&]() {
        Core::Evaluator::Variables vars;
        for (int rep = 0; rep < repetitions; ++rep) {
            for (int iy = 0; iy <= resolution; ++iy) {
                vars["y"] = bounds.yMin + static_cast<double>(iy) * dy;
                for (int ix = 0; ix <= resolution; ++ix) {
                    vars["x"] = bounds.xMin + static_cast<double>(ix) * dx;
                    consumeSample(Core::Evaluator::evaluate(ast, vars), checksum);
                }
            }
        }
    });

    return { ms, checksum };
}

TimedSample measureExplicitSurfaceWithSlots(const Core::ASTNodePtr& ast, int repetitions) {
    const Plotting::Geometry::Bounds2D bounds{ -6.0, 6.0, -6.0, 6.0 };
    constexpr int resolution = 96;
    const double dx = (bounds.xMax - bounds.xMin) / static_cast<double>(resolution);
    const double dy = (bounds.yMax - bounds.yMin) / static_cast<double>(resolution);

    double checksum = 0.0;
    const double ms = elapsedMilliseconds([&]() {
        Core::EvaluationContext context;
        for (int rep = 0; rep < repetitions; ++rep) {
            for (int iy = 0; iy <= resolution; ++iy) {
                context.y = bounds.yMin + static_cast<double>(iy) * dy;
                for (int ix = 0; ix <= resolution; ++ix) {
                    context.x = bounds.xMin + static_cast<double>(ix) * dx;
                    consumeSample(Core::Evaluator::evaluate(ast, context), checksum);
                }
            }
        }
    });

    return { ms, checksum };
}

TimedSample measureImplicitFieldWithMap(const Core::ASTNodePtr& ast, int repetitions) {
    const Plotting::Geometry::Bounds3D bounds{ -4.0, 4.0, -4.0, 4.0, -4.0, 4.0 };
    constexpr int resolution = 32;
    const double dx = (bounds.xMax - bounds.xMin) / static_cast<double>(resolution);
    const double dy = (bounds.yMax - bounds.yMin) / static_cast<double>(resolution);
    const double dz = (bounds.zMax - bounds.zMin) / static_cast<double>(resolution);

    double checksum = 0.0;
    const double ms = elapsedMilliseconds([&]() {
        Core::Evaluator::Variables vars;
        for (int rep = 0; rep < repetitions; ++rep) {
            for (int iz = 0; iz <= resolution; ++iz) {
                vars["z"] = bounds.zMin + static_cast<double>(iz) * dz;
                for (int iy = 0; iy <= resolution; ++iy) {
                    vars["y"] = bounds.yMin + static_cast<double>(iy) * dy;
                    for (int ix = 0; ix <= resolution; ++ix) {
                        vars["x"] = bounds.xMin + static_cast<double>(ix) * dx;
                        consumeSample(Core::Evaluator::evaluate(ast, vars), checksum);
                    }
                }
            }
        }
    });

    return { ms, checksum };
}

TimedSample measureImplicitFieldWithSlots(const Core::ASTNodePtr& ast, int repetitions) {
    const Plotting::Geometry::Bounds3D bounds{ -4.0, 4.0, -4.0, 4.0, -4.0, 4.0 };
    constexpr int resolution = 32;
    const double dx = (bounds.xMax - bounds.xMin) / static_cast<double>(resolution);
    const double dy = (bounds.yMax - bounds.yMin) / static_cast<double>(resolution);
    const double dz = (bounds.zMax - bounds.zMin) / static_cast<double>(resolution);

    double checksum = 0.0;
    const double ms = elapsedMilliseconds([&]() {
        Core::EvaluationContext context;
        for (int rep = 0; rep < repetitions; ++rep) {
            for (int iz = 0; iz <= resolution; ++iz) {
                context.z = bounds.zMin + static_cast<double>(iz) * dz;
                for (int iy = 0; iy <= resolution; ++iy) {
                    context.y = bounds.yMin + static_cast<double>(iy) * dy;
                    for (int ix = 0; ix <= resolution; ++ix) {
                        context.x = bounds.xMin + static_cast<double>(ix) * dx;
                        consumeSample(Core::Evaluator::evaluate(ast, context), checksum);
                    }
                }
            }
        }
    });

    return { ms, checksum };
}

double speedup(double beforeMs, double afterMs) noexcept {
    return afterMs > 0.0 ? beforeMs / afterMs : std::numeric_limits<double>::quiet_NaN();
}

} // namespace

TEST_CASE(ExpressionRuntimeBenchmark_OptionalSamplingTimings) {
    if (!expressionBenchmarkEnabled()) {
        return;
    }

    const Core::ASTNodePtr curveAst =
        parseBenchmarkFormula("sin(x) + 0.125*cos(3*x)");
    const Core::ASTNodePtr surfaceAst =
        parseBenchmarkFormula("sin(sqrt(x^2+y^2))");
    const Core::ASTNodePtr fieldAst =
        parseBenchmarkFormula("x^2 + y^2 + z^2 - 9");

    const TimedSample curveMap = measureCurveWithMap(curveAst, 200);
    const TimedSample curveSlots = measureCurveWithSlots(curveAst, 200);
    Assert::IsTrue(curveMap.checksum > 0.0);
    Assert::IsTrue(curveSlots.checksum > 0.0);

    const TimedSample surfaceMap = measureExplicitSurfaceWithMap(surfaceAst, 100);
    const TimedSample surfaceSlots = measureExplicitSurfaceWithSlots(surfaceAst, 100);
    Assert::IsTrue(surfaceMap.checksum > 0.0);
    Assert::IsTrue(surfaceSlots.checksum > 0.0);

    const TimedSample fieldMap = measureImplicitFieldWithMap(fieldAst, 20);
    const TimedSample fieldSlots = measureImplicitFieldWithSlots(fieldAst, 20);
    Assert::IsTrue(fieldMap.checksum > 0.0);
    Assert::IsTrue(fieldSlots.checksum > 0.0);

    std::cout << "[BENCH] expression runtime sampling, map adapter before vs fixed slots after\n"
              << "  curve: " << curveMap.milliseconds << " ms -> "
              << curveSlots.milliseconds << " ms, "
              << speedup(curveMap.milliseconds, curveSlots.milliseconds)
              << "x, 200 x 4097 samples\n"
              << "  explicit surface: " << surfaceMap.milliseconds << " ms -> "
              << surfaceSlots.milliseconds << " ms, "
              << speedup(surfaceMap.milliseconds, surfaceSlots.milliseconds)
              << "x, 100 x 97^2 samples\n"
              << "  implicit field: " << fieldMap.milliseconds << " ms -> "
              << fieldSlots.milliseconds << " ms, "
              << speedup(fieldMap.milliseconds, fieldSlots.milliseconds)
              << "x, 20 x 33^3 samples\n";
}
