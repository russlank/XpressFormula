// FunctionRegistry.h - Shared metadata for built-in expression functions.
#pragma once

#include <span>
#include <string_view>

namespace XpressFormula::Core {

enum class FunctionId {
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Atan2,
    Sinh,
    Cosh,
    Tanh,
    Sqrt,
    Cbrt,
    Abs,
    Ceil,
    Floor,
    Round,
    Log,
    Log2,
    Log10,
    Exp,
    Min,
    Max,
    Pow,
    Mod,
    Sign,
    Hypot,
    Length2,
    Length3,
    Distance2,
    Distance3,
    Clamp,
    Saturate,
    Mix,
    Lerp,
    InverseLerp,
    Remap,
    Step,
    Smoothstep,
    Smootherstep,
    Fract,
    Tri,
    Pulse,
    Repeat,
    Smin,
    Smax,
    SdSphere,
    SdBox,
    SdTorus,
    SdCylinderX,
    SdCylinderY,
    SdCylinderZ,
    Noise2,
    Noise3,
    Fbm2,
    Fbm3,
};

struct FunctionInfo {
    FunctionId id;
    const char* name;
    const char* signature;
    const char* description;
    const char* category;
    int minArity;
    int maxArity;
};

std::span<const FunctionInfo> functionRegistry();
const FunctionInfo* findFunctionInfo(std::string_view name);
bool isBuiltinFunction(std::string_view name);

} // namespace XpressFormula::Core
