// FunctionRegistry.cpp - Built-in expression function metadata.
#include "FunctionRegistry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace XpressFormula::Core {

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

bool hasNaN(std::span<const double> args) {
    for (double value : args) {
        if (std::isnan(value)) {
            return true;
        }
    }
    return false;
}

double clampValue(double x, double minValue, double maxValue) {
    if (minValue > maxValue) {
        std::swap(minValue, maxValue);
    }
    return std::min(std::max(x, minValue), maxValue);
}

double mixValue(double a, double b, double t) {
    return a * (1.0 - t) + b * t;
}

double inverseLerpValue(double a, double b, double x) {
    if (a == b) {
        return NaN;
    }
    return clampValue((x - a) / (b - a), 0.0, 1.0);
}

double smoothstepValue(double a, double b, double x) {
    const double t = inverseLerpValue(a, b, x);
    return std::isnan(t) ? NaN : t * t * (3.0 - 2.0 * t);
}

double smootherstepValue(double a, double b, double x) {
    const double t = inverseLerpValue(a, b, x);
    return std::isnan(t) ? NaN : t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double fractValue(double x) {
    return x - std::floor(x);
}

double stepValue(double edge, double x) {
    return x < edge ? 0.0 : 1.0;
}

double repeatValue(double x, double period) {
    if (period == 0.0 || !std::isfinite(x) || !std::isfinite(period)) {
        return NaN;
    }

    const double p = std::abs(period);
    const double value = p * fractValue((x / p) + 0.5) - (p * 0.5);
    return value == 0.0 ? 0.0 : value;
}

double sminValue(double a, double b, double k) {
    if (k <= 0.0) {
        return std::min(a, b);
    }

    const double h = clampValue(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mixValue(b, a, h) - k * h * (1.0 - h);
}

double smaxValue(double a, double b, double k) {
    if (k <= 0.0) {
        return std::max(a, b);
    }
    return -sminValue(-a, -b, k);
}

double sdBoxValue(double x, double y, double z, double bx, double by, double bz) {
    if (bx < 0.0 || by < 0.0 || bz < 0.0) {
        return NaN;
    }

    const double qx = std::abs(x) - bx;
    const double qy = std::abs(y) - by;
    const double qz = std::abs(z) - bz;
    const double outside = std::hypot(std::max(qx, 0.0),
                                      std::max(qy, 0.0),
                                      std::max(qz, 0.0));
    const double inside = std::min(std::max(qx, std::max(qy, qz)), 0.0);
    return outside + inside;
}

struct LatticeCoord {
    int base = 0;
    double t = 0.0;
    bool valid = false;
};

LatticeCoord latticeCoord(double value) {
    if (!std::isfinite(value)) {
        return {};
    }

    constexpr double kNoisePeriod = 1048576.0;
    const double wrapped = std::fmod(value, kNoisePeriod);
    const double base = std::floor(wrapped);
    return { static_cast<int>(base), wrapped - base, true };
}

uint32_t avalanche(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

uint32_t hashLattice(int x, int y, int z = 0) {
    uint32_t h = 0x811c9dc5U;
    h ^= static_cast<uint32_t>(x) + 0x9e3779b9U + (h << 6) + (h >> 2);
    h = avalanche(h);
    h ^= static_cast<uint32_t>(y) + 0x85ebca6bU + (h << 6) + (h >> 2);
    h = avalanche(h);
    h ^= static_cast<uint32_t>(z) + 0xc2b2ae35U + (h << 6) + (h >> 2);
    return avalanche(h);
}

double hashToSignedUnit(uint32_t h) {
    constexpr double kScale = 1.0 / static_cast<double>(std::numeric_limits<uint32_t>::max());
    return static_cast<double>(h) * kScale * 2.0 - 1.0;
}

double fade(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double noise2Value(double x, double y) {
    const LatticeCoord lx = latticeCoord(x);
    const LatticeCoord ly = latticeCoord(y);
    if (!lx.valid || !ly.valid) {
        return NaN;
    }

    const double tx = fade(lx.t);
    const double ty = fade(ly.t);
    const double v00 = hashToSignedUnit(hashLattice(lx.base,     ly.base));
    const double v10 = hashToSignedUnit(hashLattice(lx.base + 1, ly.base));
    const double v01 = hashToSignedUnit(hashLattice(lx.base,     ly.base + 1));
    const double v11 = hashToSignedUnit(hashLattice(lx.base + 1, ly.base + 1));
    const double x0 = mixValue(v00, v10, tx);
    const double x1 = mixValue(v01, v11, tx);
    return mixValue(x0, x1, ty);
}

double noise3Value(double x, double y, double z) {
    const LatticeCoord lx = latticeCoord(x);
    const LatticeCoord ly = latticeCoord(y);
    const LatticeCoord lz = latticeCoord(z);
    if (!lx.valid || !ly.valid || !lz.valid) {
        return NaN;
    }

    const double tx = fade(lx.t);
    const double ty = fade(ly.t);
    const double tz = fade(lz.t);
    const double v000 = hashToSignedUnit(hashLattice(lx.base,     ly.base,     lz.base));
    const double v100 = hashToSignedUnit(hashLattice(lx.base + 1, ly.base,     lz.base));
    const double v010 = hashToSignedUnit(hashLattice(lx.base,     ly.base + 1, lz.base));
    const double v110 = hashToSignedUnit(hashLattice(lx.base + 1, ly.base + 1, lz.base));
    const double v001 = hashToSignedUnit(hashLattice(lx.base,     ly.base,     lz.base + 1));
    const double v101 = hashToSignedUnit(hashLattice(lx.base + 1, ly.base,     lz.base + 1));
    const double v011 = hashToSignedUnit(hashLattice(lx.base,     ly.base + 1, lz.base + 1));
    const double v111 = hashToSignedUnit(hashLattice(lx.base + 1, ly.base + 1, lz.base + 1));

    const double x00 = mixValue(v000, v100, tx);
    const double x10 = mixValue(v010, v110, tx);
    const double x01 = mixValue(v001, v101, tx);
    const double x11 = mixValue(v011, v111, tx);
    const double y0 = mixValue(x00, x10, ty);
    const double y1 = mixValue(x01, x11, ty);
    return mixValue(y0, y1, tz);
}

double fbm2Value(double x, double y) {
    double total = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    double frequency = 1.0;

    for (int octave = 0; octave < 5; ++octave) {
        const double value = noise2Value(x * frequency, y * frequency);
        if (std::isnan(value)) {
            return NaN;
        }
        total += value * amplitude;
        amplitudeSum += amplitude;
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return total / amplitudeSum;
}

double fbm3Value(double x, double y, double z) {
    double total = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    double frequency = 1.0;

    for (int octave = 0; octave < 5; ++octave) {
        const double value = noise3Value(x * frequency, y * frequency, z * frequency);
        if (std::isnan(value)) {
            return NaN;
        }
        total += value * amplitude;
        amplitudeSum += amplitude;
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return total / amplitudeSum;
}

double evalSin(std::span<const double> a) { return std::sin(a[0]); }
double evalCos(std::span<const double> a) { return std::cos(a[0]); }
double evalTan(std::span<const double> a) { return std::tan(a[0]); }
double evalAsin(std::span<const double> a) { return std::asin(a[0]); }
double evalAcos(std::span<const double> a) { return std::acos(a[0]); }
double evalAtan(std::span<const double> a) { return std::atan(a[0]); }
double evalAtan2(std::span<const double> a) { return std::atan2(a[0], a[1]); }
double evalSinh(std::span<const double> a) { return std::sinh(a[0]); }
double evalCosh(std::span<const double> a) { return std::cosh(a[0]); }
double evalTanh(std::span<const double> a) { return std::tanh(a[0]); }
double evalSqrt(std::span<const double> a) { return (a[0] >= 0.0) ? std::sqrt(a[0]) : NaN; }
double evalCbrt(std::span<const double> a) { return std::cbrt(a[0]); }
double evalAbs(std::span<const double> a) { return std::abs(a[0]); }
double evalCeil(std::span<const double> a) { return std::ceil(a[0]); }
double evalFloor(std::span<const double> a) { return std::floor(a[0]); }
double evalRound(std::span<const double> a) { return std::round(a[0]); }
double evalLog(std::span<const double> a) {
    if (a.size() == 1) {
        return (a[0] > 0.0) ? std::log(a[0]) : NaN;
    }
    return (a[0] > 0.0 && a[1] > 0.0 && a[0] != 1.0)
        ? std::log(a[1]) / std::log(a[0])
        : NaN;
}
double evalLog2(std::span<const double> a) { return (a[0] > 0.0) ? std::log2(a[0]) : NaN; }
double evalLog10(std::span<const double> a) { return (a[0] > 0.0) ? std::log10(a[0]) : NaN; }
double evalExp(std::span<const double> a) { return std::exp(a[0]); }
double evalMin(std::span<const double> a) { return std::min(a[0], a[1]); }
double evalMax(std::span<const double> a) { return std::max(a[0], a[1]); }
double evalPow(std::span<const double> a) { return std::pow(a[0], a[1]); }
double evalMod(std::span<const double> a) { return (a[1] != 0.0) ? std::fmod(a[0], a[1]) : NaN; }
double evalSign(std::span<const double> a) {
    return (a[0] > 0.0) ? 1.0 : (a[0] < 0.0) ? -1.0 : 0.0;
}
double evalHypot(std::span<const double> a) { return std::hypot(a[0], a[1]); }
double evalLength2(std::span<const double> a) { return std::hypot(a[0], a[1]); }
double evalLength3(std::span<const double> a) { return std::hypot(a[0], a[1], a[2]); }
double evalDistance2(std::span<const double> a) { return std::hypot(a[0] - a[2], a[1] - a[3]); }
double evalDistance3(std::span<const double> a) {
    return std::hypot(a[0] - a[3], a[1] - a[4], a[2] - a[5]);
}
double evalClamp(std::span<const double> a) { return clampValue(a[0], a[1], a[2]); }
double evalSaturate(std::span<const double> a) { return clampValue(a[0], 0.0, 1.0); }
double evalMix(std::span<const double> a) { return mixValue(a[0], a[1], a[2]); }
double evalInverseLerp(std::span<const double> a) { return inverseLerpValue(a[0], a[1], a[2]); }
double evalRemap(std::span<const double> a) {
    const double t = inverseLerpValue(a[0], a[1], a[4]);
    return std::isnan(t) ? NaN : mixValue(a[2], a[3], t);
}
double evalStep(std::span<const double> a) { return stepValue(a[0], a[1]); }
double evalSmoothstep(std::span<const double> a) { return smoothstepValue(a[0], a[1], a[2]); }
double evalSmootherstep(std::span<const double> a) { return smootherstepValue(a[0], a[1], a[2]); }
double evalFract(std::span<const double> a) { return fractValue(a[0]); }
double evalTri(std::span<const double> a) { return std::abs(fractValue(a[0]) - 0.5) * 2.0; }
double evalPulse(std::span<const double> a) {
    double edge0 = a[0];
    double edge1 = a[1];
    if (edge0 > edge1) {
        std::swap(edge0, edge1);
    }
    return stepValue(edge0, a[2]) - stepValue(edge1, a[2]);
}
double evalRepeat(std::span<const double> a) { return repeatValue(a[0], a[1]); }
double evalSmin(std::span<const double> a) { return sminValue(a[0], a[1], a[2]); }
double evalSmax(std::span<const double> a) { return smaxValue(a[0], a[1], a[2]); }
double evalSdSphere(std::span<const double> a) {
    return (a[3] >= 0.0) ? std::hypot(a[0], a[1], a[2]) - a[3] : NaN;
}
double evalSdBox(std::span<const double> a) {
    return sdBoxValue(a[0], a[1], a[2], a[3], a[4], a[5]);
}
double evalSdTorus(std::span<const double> a) {
    return (a[3] >= 0.0 && a[4] >= 0.0)
        ? std::hypot(std::hypot(a[0], a[1]) - a[3], a[2]) - a[4]
        : NaN;
}
double evalSdCylinderX(std::span<const double> a) {
    return (a[3] >= 0.0) ? std::hypot(a[1], a[2]) - a[3] : NaN;
}
double evalSdCylinderY(std::span<const double> a) {
    return (a[3] >= 0.0) ? std::hypot(a[0], a[2]) - a[3] : NaN;
}
double evalSdCylinderZ(std::span<const double> a) {
    return (a[3] >= 0.0) ? std::hypot(a[0], a[1]) - a[3] : NaN;
}
double evalNoise2(std::span<const double> a) { return noise2Value(a[0], a[1]); }
double evalNoise3(std::span<const double> a) { return noise3Value(a[0], a[1], a[2]); }
double evalFbm2(std::span<const double> a) { return fbm2Value(a[0], a[1]); }
double evalFbm3(std::span<const double> a) { return fbm3Value(a[0], a[1], a[2]); }

constexpr auto kFunctions = std::to_array<FunctionInfo>({
    { FunctionId::Sin, "sin", "sin(a)", "sine of an angle in radians", "Trigonometry",
      "Computes the sine of an angle measured in radians. Useful for waves, oscillation, and periodic surface detail.",
      "standard trigonometric sine", "sin(x)", 1, 1, evalSin },
    { FunctionId::Cos, "cos", "cos(a)", "cosine of an angle in radians", "Trigonometry",
      "Computes the cosine of an angle measured in radians. Useful for phase-shifted waves, circular motion, and periodic patterns.",
      "standard trigonometric cosine", "cos(x)", 1, 1, evalCos },
    { FunctionId::Tan, "tan", "tan(a)", "tangent of an angle in radians", "Trigonometry",
      "Computes tangent for an angle measured in radians. Values grow rapidly near odd multiples of pi/2.",
      "sin(a)/cos(a)", "tan(x)", 1, 1, evalTan },
    { FunctionId::Asin, "asin", "asin(a)", "inverse sine, in radians", "Trigonometry",
      "Returns the angle whose sine is a. The real-valued domain is -1 to 1.",
      "inverse of sin(a) over its principal branch", "asin(saturate(x))", 1, 1, evalAsin },
    { FunctionId::Acos, "acos", "acos(a)", "inverse cosine, in radians", "Trigonometry",
      "Returns the angle whose cosine is a. The real-valued domain is -1 to 1.",
      "inverse of cos(a) over its principal branch", "acos(saturate(x))", 1, 1, evalAcos },
    { FunctionId::Atan, "atan", "atan(a)", "inverse tangent, in radians", "Trigonometry",
      "Returns the principal inverse tangent of a value. Useful for angle calculations from slopes.",
      "inverse of tan(a) over its principal branch", "atan(x)", 1, 1, evalAtan },
    { FunctionId::Atan2, "atan2", "atan2(y,x)", "angle of the vector from x/y components", "Trigonometry",
      "Returns the signed angle of the 2D vector (x,y), preserving quadrant information. Useful for radial patterns.",
      "quadrant-aware atan(y/x)", "atan2(y,x)", 2, 2, evalAtan2 },
    { FunctionId::Sinh, "sinh", "sinh(a)", "hyperbolic sine", "Trigonometry",
      "Computes hyperbolic sine. It grows exponentially and can create steep analytic curves.",
      "(exp(a)-exp(-a))/2", "sinh(x)", 1, 1, evalSinh },
    { FunctionId::Cosh, "cosh", "cosh(a)", "hyperbolic cosine", "Trigonometry",
      "Computes hyperbolic cosine. It is symmetric and useful for catenary-like curves.",
      "(exp(a)+exp(-a))/2", "cosh(x)-2", 1, 1, evalCosh },
    { FunctionId::Tanh, "tanh", "tanh(a)", "hyperbolic tangent", "Trigonometry",
      "Computes a smooth S-shaped transition from -1 to 1. Useful for soft limiting.",
      "sinh(a)/cosh(a)", "tanh(x)", 1, 1, evalTanh },

    { FunctionId::Sqrt, "sqrt", "sqrt(a)", "square root, NaN for negative input", "Basic",
      "Returns the non-negative square root. Negative inputs return NaN in the real-valued evaluator.",
      "a^0.5, for a >= 0", "sqrt(abs(x))", 1, 1, evalSqrt },
    { FunctionId::Cbrt, "cbrt", "cbrt(a)", "cube root", "Basic",
      "Returns the real cube root, including for negative values.",
      "real-valued cube root of a", "cbrt(x)", 1, 1, evalCbrt },
    { FunctionId::Abs, "abs", "abs(a)", "absolute value", "Basic",
      "Returns distance from zero on the number line. Useful for symmetry and folded shapes.",
      "a < 0 ? -a : a", "abs(x)-1", 1, 1, evalAbs },
    { FunctionId::Ceil, "ceil", "ceil(a)", "round upward to an integer", "Basic",
      "Rounds a value up to the next integer. Useful for stepped patterns.",
      "smallest integer greater than or equal to a", "ceil(x)", 1, 1, evalCeil },
    { FunctionId::Floor, "floor", "floor(a)", "round downward to an integer", "Basic",
      "Rounds a value down to the previous integer. Useful for cells, steps, and fractional patterns.",
      "largest integer less than or equal to a", "floor(x)", 1, 1, evalFloor },
    { FunctionId::Round, "round", "round(a)", "round to nearest integer", "Basic",
      "Rounds to the nearest integer, with half values rounded away from zero.",
      "nearest integer to a", "round(x)", 1, 1, evalRound },
    { FunctionId::Log, "log", "log(a) or log(base,value)", "natural log or base-log form", "Basic",
      "Computes the natural logarithm with one argument, or a logarithm in a custom base with two arguments.",
      "log(value)/log(base) for the two-argument form", "log(2,abs(x)+1)", 1, 2, evalLog },
    { FunctionId::Log2, "log2", "log2(a)", "base-2 logarithm", "Basic",
      "Computes the logarithm in base 2. Inputs must be positive.",
      "log(a)/log(2)", "log2(abs(x)+1)", 1, 1, evalLog2 },
    { FunctionId::Log10, "log10", "log10(a)", "base-10 logarithm", "Basic",
      "Computes the logarithm in base 10. Inputs must be positive.",
      "log(a)/log(10)", "log10(abs(x)+1)", 1, 1, evalLog10 },
    { FunctionId::Exp, "exp", "exp(a)", "e raised to a power", "Basic",
      "Raises Euler's number to a power. Useful for decay curves and growth effects.",
      "e^a", "exp(-x*x)", 1, 1, evalExp },
    { FunctionId::Min, "min", "min(a,b)", "smaller of two values", "Basic",
      "Returns the smaller of two values. Useful for choosing lower fields or combining hard shape unions.",
      "a when a < b, otherwise b", "min(x,y)", 2, 2, evalMin },
    { FunctionId::Max, "max", "max(a,b)", "larger of two values", "Basic",
      "Returns the larger of two values. Useful for hard intersections and clipping implicit shapes.",
      "a when a > b, otherwise b", "max(x,y)", 2, 2, evalMax },
    { FunctionId::Pow, "pow", "pow(a,b)", "a raised to power b", "Basic",
      "Raises one value to another. Use integer powers for stable polynomial shapes.",
      "a^b", "pow(abs(x),4)", 2, 2, evalPow },
    { FunctionId::Mod, "mod", "mod(a,b)", "floating-point remainder, NaN when b is zero", "Basic",
      "Returns the floating-point remainder after dividing a by b. The result keeps the sign of a.",
      "floating-point remainder of a divided by b", "mod(x,1)", 2, 2, evalMod },
    { FunctionId::Sign, "sign", "sign(a)", "-1, 0, or 1 depending on the sign", "Basic",
      "Classifies a value as negative, zero, or positive. Useful for simple discontinuous masks.",
      "-1 if a < 0, 0 if a == 0, 1 if a > 0", "sign(x)", 1, 1, evalSign },

    { FunctionId::Hypot, "hypot", "hypot(a,b)", "2D distance from the origin", "Distance",
      "Computes a stable 2D length from two components. It is equivalent to length2 with shorter argument names.",
      "sqrt(a^2 + b^2)", "hypot(x,y)", 2, 2, evalHypot },
    { FunctionId::Length2, "length2", "length2(x,y)", "2D vector length", "Distance",
      "Computes distance from the origin to the point (x,y). Useful for circles, radial waves, and falloff.",
      "sqrt(x^2 + y^2)", "sin(8*length2(x,y))", 2, 2, evalLength2 },
    { FunctionId::Length3, "length3", "length3(x,y,z)", "3D vector length", "Distance",
      "Computes distance from the origin to the point (x,y,z). Useful for spheres, radial waves, and implicit 3D surfaces.",
      "sqrt(x^2 + y^2 + z^2)", "length3(x,y,z)-1", 3, 3, evalLength3 },
    { FunctionId::Distance2, "distance2", "distance2(x1,y1,x2,y2)", "distance between two 2D points", "Distance",
      "Computes the distance between two 2D points. Useful for placing radial effects around custom centers.",
      "length2(x1-x2, y1-y2)", "distance2(x,y,1,1)", 4, 4, evalDistance2 },
    { FunctionId::Distance3, "distance3", "distance3(x1,y1,z1,x2,y2,z2)", "distance between two 3D points", "Distance",
      "Computes the distance between two 3D points. Useful for fields centered away from the origin.",
      "length3(x1-x2, y1-y2, z1-z2)", "distance3(x,y,z,1,0,0)-0.5", 6, 6, evalDistance3 },

    { FunctionId::Clamp, "clamp", "clamp(x,min,max)", "limit a value to a range", "Range / interpolation",
      "Limits a value to a range. Reversed bounds are swapped for convenience.",
      "min(max(x,min),max), with reversed bounds swapped", "clamp(x,0,1)", 3, 3, evalClamp },
    { FunctionId::Saturate, "saturate", "saturate(x)", "clamp a value to 0..1", "Range / interpolation",
      "Clamps a value into the normalized 0 to 1 range. Useful for masks and blend weights.",
      "clamp(x,0,1)", "saturate(x)", 1, 1, evalSaturate },
    { FunctionId::Mix, "mix", "mix(a,b,t)", "linear blend from a to b; t may extrapolate", "Range / interpolation",
      "Linearly blends between a and b. The blend factor t is not clamped, so extrapolation is allowed.",
      "a*(1-t)+b*t", "mix(sin(x),cos(x),0.5)", 3, 3, evalMix },
    { FunctionId::Lerp, "lerp", "lerp(a,b,t)", "alias of mix(a,b,t)", "Range / interpolation",
      "Alias for mix. Use whichever name is clearer for the formula you are writing.",
      "mix(a,b,t)", "lerp(0,1,saturate(x))", 3, 3, evalMix },
    { FunctionId::InverseLerp, "inverseLerp", "inverseLerp(a,b,x)", "normalized position of x within a..b", "Range / interpolation",
      "Maps x to a clamped 0 to 1 value based on its position between a and b. Equal endpoints return NaN.",
      "clamp((x-a)/(b-a),0,1)", "inverseLerp(-1,1,x)", 3, 3, evalInverseLerp },
    { FunctionId::Remap, "remap", "remap(inMin,inMax,outMin,outMax,x)", "map x from one range into another", "Range / interpolation",
      "Maps x from one range into another using a clamped normalized position. Equal input endpoints return NaN.",
      "mix(outMin,outMax,inverseLerp(inMin,inMax,x))", "remap(-1,1,0,10,x)", 5, 5, evalRemap },
    { FunctionId::Step, "step", "step(edge,x)", "0 before edge, 1 at or after edge", "Range / interpolation",
      "Creates a hard threshold. It returns 0 when x is less than edge and 1 otherwise.",
      "x < edge ? 0 : 1", "step(0,x)", 2, 2, evalStep },
    { FunctionId::Smoothstep, "smoothstep", "smoothstep(a,b,x)", "smooth clamped transition from 0 to 1", "Range / interpolation",
      "Returns a smooth transition between two edge values. Useful for soft masks, falloff, and visual blending.",
      "t*t*(3-2*t), where t = clamp((x-a)/(b-a),0,1)", "sin(8*length2(x,y))*smoothstep(3,0,length2(x,y))", 3, 3, evalSmoothstep },
    { FunctionId::Smootherstep, "smootherstep", "smootherstep(a,b,x)", "extra-smooth clamped transition", "Range / interpolation",
      "Returns a smoother transition with zero first and second derivatives at the endpoints.",
      "t^3*(t*(t*6-15)+10), where t = clamp((x-a)/(b-a),0,1)", "smootherstep(0,1,saturate(x))", 3, 3, evalSmootherstep },

    { FunctionId::Fract, "fract", "fract(x)", "fractional part using x-floor(x)", "Patterns",
      "Returns the fractional part of x. Negative values wrap upward, so fract(-0.25) is 0.75.",
      "x - floor(x)", "fract(x)", 1, 1, evalFract },
    { FunctionId::Tri, "tri", "tri(x)", "triangle wave from fractional position", "Patterns",
      "Creates a repeating triangle wave based on the fractional part of x. The phase is 1 at integer values.",
      "abs(fract(x)-0.5)*2", "tri(x)", 1, 1, evalTri },
    { FunctionId::Pulse, "pulse", "pulse(a,b,x)", "1 inside an edge interval, otherwise 0", "Patterns",
      "Creates a rectangular pulse between two edge values. Reversed edges are swapped for convenience.",
      "step(a,x)-step(b,x), with reversed edges swapped", "pulse(0.25,0.75,fract(x))", 3, 3, evalPulse },
    { FunctionId::Repeat, "repeat", "repeat(x,period)", "centered repeat coordinate", "Patterns",
      "Repeats a coordinate around zero for tileable formulas. A zero period returns NaN.",
      "mod(x + period/2, period) - period/2", "repeat(x,1)", 2, 2, evalRepeat },

    { FunctionId::Smin, "smin", "smin(a,b,k)", "smooth minimum for blending implicit shapes", "Implicit composition",
      "Blends two signed-distance or implicit fields smoothly. Useful for soft unions between shapes.",
      "h=clamp(0.5+0.5*(b-a)/k,0,1); mix(b,a,h)-k*h*(1-h)", "smin(sdSphere(x-0.7,y,z,0.5),sdSphere(x+0.7,y,z,0.5),0.25)", 3, 3, evalSmin },
    { FunctionId::Smax, "smax", "smax(a,b,k)", "smooth maximum for subtracting/blending shapes", "Implicit composition",
      "Smoothly blends maximum operations. Useful for soft intersections and soft subtraction with negated fields.",
      "-smin(-a,-b,k)", "smax(sdBox(x,y,z,1,1,1),-sdCylinderX(x,y,z,0.32),0.12)", 3, 3, evalSmax },

    { FunctionId::SdSphere, "sdSphere", "sdSphere(x,y,z,r)", "signed distance to a centered sphere", "Signed distance",
      "Returns the signed distance to a sphere centered at the origin. Negative values are inside the sphere.",
      "length3(x,y,z)-r", "sdSphere(x,y,z,1)", 4, 4, evalSdSphere },
    { FunctionId::SdBox, "sdBox", "sdBox(x,y,z,bx,by,bz)", "signed distance to a centered box", "Signed distance",
      "Returns signed distance to an axis-aligned box centered at the origin with half-extents bx, by, and bz.",
      "standard centered box SDF using abs, max, min, and length3", "sdBox(x,y,z,1,1,1)", 6, 6, evalSdBox },
    { FunctionId::SdTorus, "sdTorus", "sdTorus(x,y,z,R,r)", "signed distance to a centered torus", "Signed distance",
      "Returns signed distance to a torus around the Z axis. R is the major radius and r is the tube radius.",
      "length2(length2(x,y)-R,z)-r", "sdTorus(x,y,z,1.2,0.25)", 5, 5, evalSdTorus },
    { FunctionId::SdCylinderX, "sdCylinderX", "sdCylinderX(x,y,z,r)", "signed distance to a cylinder along X", "Signed distance",
      "Returns signed distance to an infinite cylinder whose axis follows X.",
      "length2(y,z)-r", "sdCylinderX(x,y,z,0.35)", 4, 4, evalSdCylinderX },
    { FunctionId::SdCylinderY, "sdCylinderY", "sdCylinderY(x,y,z,r)", "signed distance to a cylinder along Y", "Signed distance",
      "Returns signed distance to an infinite cylinder whose axis follows Y.",
      "length2(x,z)-r", "sdCylinderY(x,y,z,0.35)", 4, 4, evalSdCylinderY },
    { FunctionId::SdCylinderZ, "sdCylinderZ", "sdCylinderZ(x,y,z,r)", "signed distance to a cylinder along Z", "Signed distance",
      "Returns signed distance to an infinite cylinder whose axis follows Z.",
      "length2(x,y)-r", "sdCylinderZ(x,y,z,0.35)", 4, 4, evalSdCylinderZ },

    { FunctionId::Noise2, "noise2", "noise2(x,y)", "deterministic 2D value noise, about -1..1", "Noise",
      "Returns deterministic 2D procedural value noise. It has no global random state and is useful for texture-like variation.",
      "Deterministic procedural noise; not expressible as a short formula using the basic functions.", "noise2(x,y)", 2, 2, evalNoise2 },
    { FunctionId::Noise3, "noise3", "noise3(x,y,z)", "deterministic 3D value noise, about -1..1", "Noise",
      "Returns deterministic 3D procedural value noise. Useful for bumpy implicit surfaces and volumetric variation.",
      "Deterministic procedural noise; not expressible as a short formula using the basic functions.", "noise3(x,y,z)", 3, 3, evalNoise3 },
    { FunctionId::Fbm2, "fbm2", "fbm2(x,y)", "normalized multi-octave 2D noise, about -1..1", "Noise",
      "Combines several octaves of deterministic 2D noise into smoother fractal variation for terrain-like surfaces.",
      "Deterministic multi-octave noise; not expressible as a short formula using the basic functions.", "fbm2(x,y)", 2, 2, evalFbm2 },
    { FunctionId::Fbm3, "fbm3", "fbm3(x,y,z)", "normalized multi-octave 3D noise, about -1..1", "Noise",
      "Combines several octaves of deterministic 3D noise into normalized fractal variation for organic implicit fields.",
      "Deterministic multi-octave noise; not expressible as a short formula using the basic functions.", "fbm3(x,y,z)", 3, 3, evalFbm3 },
});

} // namespace

std::span<const FunctionInfo> functionRegistry() {
    return kFunctions;
}

const FunctionInfo* findFunctionInfo(std::string_view name) {
    for (const FunctionInfo& info : kFunctions) {
        if (std::string_view(info.name) == name) {
            return &info;
        }
    }
    return nullptr;
}

bool isBuiltinFunction(std::string_view name) {
    return findFunctionInfo(name) != nullptr;
}

bool functionAcceptsArity(const FunctionInfo& info, std::size_t arity) noexcept {
    return arity >= static_cast<std::size_t>(info.minArity) &&
           arity <= static_cast<std::size_t>(info.maxArity);
}

double evaluateFunction(const FunctionInfo& info, std::span<const double> args) {
    if (!info.evaluate || !functionAcceptsArity(info, args.size()) || hasNaN(args)) {
        return NaN;
    }
    return info.evaluate(args);
}

double evaluateFunction(std::string_view name, std::span<const double> args) {
    const FunctionInfo* info = findFunctionInfo(name);
    return info ? evaluateFunction(*info, args) : NaN;
}

} // namespace XpressFormula::Core
