// InputLimits.h - Shared resource limits for untrusted or user-provided input.
#pragma once

#include <cstddef>
#include <cstdint>

namespace XpressFormula::Core::InputLimits {

inline constexpr std::size_t kMaxFormulaLength = 256ull * 1024ull;
inline constexpr std::size_t kMaxExpressionTokens = 100000;
inline constexpr std::size_t kMaxExpressionNesting = 512;
inline constexpr std::size_t kMaxAstNodes = 100000;
inline constexpr std::size_t kMaxAstDepth = 512;

inline constexpr std::size_t kMaxJsonDepth = 256;
inline constexpr std::size_t kMaxJsonValues = 131072;

inline constexpr std::uintmax_t kMaxProjectFileBytes = 32ull * 1024ull * 1024ull;
inline constexpr std::size_t kMaxProjectFormulas = 10000;

inline constexpr std::size_t kMaxScalarGridCells = 1'048'576;
inline constexpr std::size_t kMaxScalarLatticePoints = 1'050'625;
inline constexpr int kMaxCurveSamples = 1'000'000;

inline constexpr double kMinViewScale = 0.1;
inline constexpr double kMaxViewScale = 100000.0;
inline constexpr std::size_t kMaxHttpResponseBytes = 2ull * 1024ull * 1024ull;
inline constexpr std::size_t kMaxImageBufferBytes = 8192ull * 8192ull * 4ull;

} // namespace XpressFormula::Core::InputLimits
