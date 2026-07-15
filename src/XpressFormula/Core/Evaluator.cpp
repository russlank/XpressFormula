// Evaluator.cpp - AST evaluation with built-in math function dispatch.
#include "Evaluator.h"
#include "FunctionRegistry.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace XpressFormula::Core {

// Single canonical NaN used for all invalid evaluation paths.
static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

namespace {

bool hasNaN(const std::vector<double>& args) {
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

} // namespace

double Evaluator::evaluate(const ASTNodePtr& node, const Variables& vars) {
    if (!node) return NaN;

    switch (node->type()) {
        case NodeType::Number:
            return static_cast<NumberNode*>(node.get())->value;

        case NodeType::Variable: {
            const auto& name = static_cast<VariableNode*>(node.get())->name;
            auto it = vars.find(name);
            // Variables not present in the evaluation context are treated as invalid.
            return (it != vars.end()) ? it->second : NaN;
        }

        case NodeType::BinaryOp: {
            auto* bin = static_cast<BinaryOpNode*>(node.get());
            double l = evaluate(bin->left,  vars);
            double r = evaluate(bin->right, vars);
            switch (bin->op) {
                case BinaryOperator::Add:      return l + r;
                case BinaryOperator::Subtract: return l - r;
                case BinaryOperator::Multiply: return l * r;
                case BinaryOperator::Divide:
                    // Keep undefined operations explicit for renderer-side filtering.
                    return (r == 0.0) ? NaN : l / r;
                case BinaryOperator::Power:
                    return std::pow(l, r);
            }
            return NaN;
        }

        case NodeType::UnaryOp: {
            auto* un = static_cast<UnaryOpNode*>(node.get());
            double val = evaluate(un->operand, vars);
            switch (un->op) {
                case UnaryOperator::Negate: return -val;
                case UnaryOperator::Plus:   return val;
            }
            return NaN;
        }

        case NodeType::FunctionCall: {
            auto* fn = static_cast<FunctionCallNode*>(node.get());
            std::vector<double> args;
            args.reserve(fn->arguments.size());
            for (const auto& arg : fn->arguments)
                args.push_back(evaluate(arg, vars));
            return evaluateFunction(fn->name, args);
        }
    }
    return NaN;
}

double Evaluator::evaluateFunction(const std::string& name,
                                   const std::vector<double>& args) {
    const FunctionInfo* info = findFunctionInfo(name);
    if (info == nullptr) {
        return NaN;
    }

    const int arity = static_cast<int>(args.size());
    if (arity < info->minArity || arity > info->maxArity || hasNaN(args)) {
        return NaN;
    }

    switch (info->id) {
        case FunctionId::Sin:          return std::sin(args[0]);
        case FunctionId::Cos:          return std::cos(args[0]);
        case FunctionId::Tan:          return std::tan(args[0]);
        case FunctionId::Asin:         return std::asin(args[0]);
        case FunctionId::Acos:         return std::acos(args[0]);
        case FunctionId::Atan:         return std::atan(args[0]);
        case FunctionId::Atan2:        return std::atan2(args[0], args[1]);
        case FunctionId::Sinh:         return std::sinh(args[0]);
        case FunctionId::Cosh:         return std::cosh(args[0]);
        case FunctionId::Tanh:         return std::tanh(args[0]);
        case FunctionId::Sqrt:         return (args[0] >= 0.0) ? std::sqrt(args[0]) : NaN;
        case FunctionId::Cbrt:         return std::cbrt(args[0]);
        case FunctionId::Abs:          return std::abs(args[0]);
        case FunctionId::Ceil:         return std::ceil(args[0]);
        case FunctionId::Floor:        return std::floor(args[0]);
        case FunctionId::Round:        return std::round(args[0]);
        case FunctionId::Log:
            if (arity == 1) {
                return (args[0] > 0.0) ? std::log(args[0]) : NaN;
            }
            return (args[0] > 0.0 && args[1] > 0.0 && args[0] != 1.0)
                ? std::log(args[1]) / std::log(args[0])
                : NaN;
        case FunctionId::Log2:         return (args[0] > 0.0) ? std::log2(args[0]) : NaN;
        case FunctionId::Log10:        return (args[0] > 0.0) ? std::log10(args[0]) : NaN;
        case FunctionId::Exp:          return std::exp(args[0]);
        case FunctionId::Min:          return std::min(args[0], args[1]);
        case FunctionId::Max:          return std::max(args[0], args[1]);
        case FunctionId::Pow:          return std::pow(args[0], args[1]);
        case FunctionId::Mod:          return (args[1] != 0.0) ? std::fmod(args[0], args[1]) : NaN;
        case FunctionId::Sign:         return (args[0] > 0.0) ? 1.0 : (args[0] < 0.0) ? -1.0 : 0.0;

        case FunctionId::Hypot:
        case FunctionId::Length2:      return std::hypot(args[0], args[1]);
        case FunctionId::Length3:      return std::hypot(args[0], args[1], args[2]);
        case FunctionId::Distance2:    return std::hypot(args[0] - args[2], args[1] - args[3]);
        case FunctionId::Distance3:    return std::hypot(args[0] - args[3], args[1] - args[4], args[2] - args[5]);

        case FunctionId::Clamp:        return clampValue(args[0], args[1], args[2]);
        case FunctionId::Saturate:     return clampValue(args[0], 0.0, 1.0);
        case FunctionId::Mix:
        case FunctionId::Lerp:         return mixValue(args[0], args[1], args[2]);
        case FunctionId::InverseLerp:  return inverseLerpValue(args[0], args[1], args[2]);
        case FunctionId::Remap: {
            const double t = inverseLerpValue(args[0], args[1], args[4]);
            return std::isnan(t) ? NaN : mixValue(args[2], args[3], t);
        }
        case FunctionId::Step:         return stepValue(args[0], args[1]);
        case FunctionId::Smoothstep:   return smoothstepValue(args[0], args[1], args[2]);
        case FunctionId::Smootherstep: return smootherstepValue(args[0], args[1], args[2]);

        case FunctionId::Fract:        return fractValue(args[0]);
        case FunctionId::Tri:          return std::abs(fractValue(args[0]) - 0.5) * 2.0;
        case FunctionId::Pulse: {
            double edge0 = args[0];
            double edge1 = args[1];
            if (edge0 > edge1) {
                std::swap(edge0, edge1);
            }
            return stepValue(edge0, args[2]) - stepValue(edge1, args[2]);
        }
        case FunctionId::Repeat:       return repeatValue(args[0], args[1]);

        case FunctionId::Smin:         return sminValue(args[0], args[1], args[2]);
        case FunctionId::Smax:         return smaxValue(args[0], args[1], args[2]);

        case FunctionId::SdSphere:
            return (args[3] >= 0.0) ? std::hypot(args[0], args[1], args[2]) - args[3] : NaN;
        case FunctionId::SdBox:
            return sdBoxValue(args[0], args[1], args[2], args[3], args[4], args[5]);
        case FunctionId::SdTorus:
            return (args[3] >= 0.0 && args[4] >= 0.0)
                ? std::hypot(std::hypot(args[0], args[1]) - args[3], args[2]) - args[4]
                : NaN;
        case FunctionId::SdCylinderX:
            return (args[3] >= 0.0) ? std::hypot(args[1], args[2]) - args[3] : NaN;
        case FunctionId::SdCylinderY:
            return (args[3] >= 0.0) ? std::hypot(args[0], args[2]) - args[3] : NaN;
        case FunctionId::SdCylinderZ:
            return (args[3] >= 0.0) ? std::hypot(args[0], args[1]) - args[3] : NaN;

        case FunctionId::Noise2:       return noise2Value(args[0], args[1]);
        case FunctionId::Noise3:       return noise3Value(args[0], args[1], args[2]);
        case FunctionId::Fbm2:         return fbm2Value(args[0], args[1]);
        case FunctionId::Fbm3:         return fbm3Value(args[0], args[1], args[2]);
    }

    return NaN;
}

} // namespace XpressFormula::Core
