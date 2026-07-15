// FunctionRegistry.cpp - Built-in expression function metadata.
#include "FunctionRegistry.h"

#include <array>

namespace XpressFormula::Core {

namespace {

constexpr auto kFunctions = std::to_array<FunctionInfo>({
    { FunctionId::Sin, "sin", "sin(a)", "sine of an angle in radians", "Trigonometry", 1, 1 },
    { FunctionId::Cos, "cos", "cos(a)", "cosine of an angle in radians", "Trigonometry", 1, 1 },
    { FunctionId::Tan, "tan", "tan(a)", "tangent of an angle in radians", "Trigonometry", 1, 1 },
    { FunctionId::Asin, "asin", "asin(a)", "inverse sine, in radians", "Trigonometry", 1, 1 },
    { FunctionId::Acos, "acos", "acos(a)", "inverse cosine, in radians", "Trigonometry", 1, 1 },
    { FunctionId::Atan, "atan", "atan(a)", "inverse tangent, in radians", "Trigonometry", 1, 1 },
    { FunctionId::Atan2, "atan2", "atan2(y,x)", "angle of the vector from x/y components", "Trigonometry", 2, 2 },
    { FunctionId::Sinh, "sinh", "sinh(a)", "hyperbolic sine", "Trigonometry", 1, 1 },
    { FunctionId::Cosh, "cosh", "cosh(a)", "hyperbolic cosine", "Trigonometry", 1, 1 },
    { FunctionId::Tanh, "tanh", "tanh(a)", "hyperbolic tangent", "Trigonometry", 1, 1 },

    { FunctionId::Sqrt, "sqrt", "sqrt(a)", "square root, NaN for negative input", "Basic", 1, 1 },
    { FunctionId::Cbrt, "cbrt", "cbrt(a)", "cube root", "Basic", 1, 1 },
    { FunctionId::Abs, "abs", "abs(a)", "absolute value", "Basic", 1, 1 },
    { FunctionId::Ceil, "ceil", "ceil(a)", "round upward to an integer", "Basic", 1, 1 },
    { FunctionId::Floor, "floor", "floor(a)", "round downward to an integer", "Basic", 1, 1 },
    { FunctionId::Round, "round", "round(a)", "round to nearest integer", "Basic", 1, 1 },
    { FunctionId::Log, "log", "log(a) or log(base,value)", "natural log or base-log form", "Basic", 1, 2 },
    { FunctionId::Log2, "log2", "log2(a)", "base-2 logarithm", "Basic", 1, 1 },
    { FunctionId::Log10, "log10", "log10(a)", "base-10 logarithm", "Basic", 1, 1 },
    { FunctionId::Exp, "exp", "exp(a)", "e raised to a power", "Basic", 1, 1 },
    { FunctionId::Min, "min", "min(a,b)", "smaller of two values", "Basic", 2, 2 },
    { FunctionId::Max, "max", "max(a,b)", "larger of two values", "Basic", 2, 2 },
    { FunctionId::Pow, "pow", "pow(a,b)", "a raised to power b", "Basic", 2, 2 },
    { FunctionId::Mod, "mod", "mod(a,b)", "floating-point remainder, NaN when b is zero", "Basic", 2, 2 },
    { FunctionId::Sign, "sign", "sign(a)", "-1, 0, or 1 depending on the sign", "Basic", 1, 1 },

    { FunctionId::Hypot, "hypot", "hypot(a,b)", "2D distance from the origin", "Distance", 2, 2 },
    { FunctionId::Length2, "length2", "length2(x,y)", "2D vector length", "Distance", 2, 2 },
    { FunctionId::Length3, "length3", "length3(x,y,z)", "3D vector length", "Distance", 3, 3 },
    { FunctionId::Distance2, "distance2", "distance2(x1,y1,x2,y2)", "distance between two 2D points", "Distance", 4, 4 },
    { FunctionId::Distance3, "distance3", "distance3(x1,y1,z1,x2,y2,z2)", "distance between two 3D points", "Distance", 6, 6 },

    { FunctionId::Clamp, "clamp", "clamp(x,min,max)", "limit a value to a range", "Range / interpolation", 3, 3 },
    { FunctionId::Saturate, "saturate", "saturate(x)", "clamp a value to 0..1", "Range / interpolation", 1, 1 },
    { FunctionId::Mix, "mix", "mix(a,b,t)", "linear blend from a to b; t may extrapolate", "Range / interpolation", 3, 3 },
    { FunctionId::Lerp, "lerp", "lerp(a,b,t)", "alias of mix(a,b,t)", "Range / interpolation", 3, 3 },
    { FunctionId::InverseLerp, "inverseLerp", "inverseLerp(a,b,x)", "normalized position of x within a..b", "Range / interpolation", 3, 3 },
    { FunctionId::Remap, "remap", "remap(inMin,inMax,outMin,outMax,x)", "map x from one range into another", "Range / interpolation", 5, 5 },
    { FunctionId::Step, "step", "step(edge,x)", "0 before edge, 1 at or after edge", "Range / interpolation", 2, 2 },
    { FunctionId::Smoothstep, "smoothstep", "smoothstep(a,b,x)", "smooth clamped transition from 0 to 1", "Range / interpolation", 3, 3 },
    { FunctionId::Smootherstep, "smootherstep", "smootherstep(a,b,x)", "extra-smooth clamped transition", "Range / interpolation", 3, 3 },

    { FunctionId::Fract, "fract", "fract(x)", "fractional part using x-floor(x)", "Patterns", 1, 1 },
    { FunctionId::Tri, "tri", "tri(x)", "triangle wave from fractional position", "Patterns", 1, 1 },
    { FunctionId::Pulse, "pulse", "pulse(a,b,x)", "1 inside an edge interval, otherwise 0", "Patterns", 3, 3 },
    { FunctionId::Repeat, "repeat", "repeat(x,period)", "centered repeat coordinate", "Patterns", 2, 2 },

    { FunctionId::Smin, "smin", "smin(a,b,k)", "smooth minimum for blending implicit shapes", "Implicit composition", 3, 3 },
    { FunctionId::Smax, "smax", "smax(a,b,k)", "smooth maximum for subtracting/blending shapes", "Implicit composition", 3, 3 },

    { FunctionId::SdSphere, "sdSphere", "sdSphere(x,y,z,r)", "signed distance to a centered sphere", "Signed distance", 4, 4 },
    { FunctionId::SdBox, "sdBox", "sdBox(x,y,z,bx,by,bz)", "signed distance to a centered box", "Signed distance", 6, 6 },
    { FunctionId::SdTorus, "sdTorus", "sdTorus(x,y,z,R,r)", "signed distance to a centered torus", "Signed distance", 5, 5 },
    { FunctionId::SdCylinderX, "sdCylinderX", "sdCylinderX(x,y,z,r)", "signed distance to a cylinder along X", "Signed distance", 4, 4 },
    { FunctionId::SdCylinderY, "sdCylinderY", "sdCylinderY(x,y,z,r)", "signed distance to a cylinder along Y", "Signed distance", 4, 4 },
    { FunctionId::SdCylinderZ, "sdCylinderZ", "sdCylinderZ(x,y,z,r)", "signed distance to a cylinder along Z", "Signed distance", 4, 4 },

    { FunctionId::Noise2, "noise2", "noise2(x,y)", "deterministic 2D value noise, about -1..1", "Noise", 2, 2 },
    { FunctionId::Noise3, "noise3", "noise3(x,y,z)", "deterministic 3D value noise, about -1..1", "Noise", 3, 3 },
    { FunctionId::Fbm2, "fbm2", "fbm2(x,y)", "normalized multi-octave 2D noise, about -1..1", "Noise", 2, 2 },
    { FunctionId::Fbm3, "fbm3", "fbm3(x,y,z)", "normalized multi-octave 3D noise, about -1..1", "Noise", 3, 3 },
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

} // namespace XpressFormula::Core
