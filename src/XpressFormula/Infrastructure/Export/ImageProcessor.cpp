// SPDX-License-Identifier: MIT
// ImageProcessor.cpp - Pure pixel operations used by export preview and output.
#include "ImageProcessor.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace XpressFormula::Infrastructure::Export {

void resizePixelsBilinear(std::span<const std::uint8_t> srcPixels,
                          int srcWidth,
                          int srcHeight,
                          int dstWidth,
                          int dstHeight,
                          std::vector<std::uint8_t>& dstPixels) {
    dstPixels.clear();
    if (srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
        return;
    }

    const size_t expectedBytes = static_cast<size_t>(srcWidth) *
        static_cast<size_t>(srcHeight) * 4u;
    if (srcPixels.size() < expectedBytes) {
        return;
    }

    if (srcWidth == dstWidth && srcHeight == dstHeight) {
        dstPixels.assign(srcPixels.begin(), srcPixels.begin() + expectedBytes);
        return;
    }

    dstPixels.resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight) * 4u);

    const auto sampleIndex = [srcWidth](int x, int y) {
        return (static_cast<size_t>(y) * static_cast<size_t>(srcWidth) +
                static_cast<size_t>(x)) * 4u;
    };

    for (int y = 0; y < dstHeight; ++y) {
        double srcY = ((static_cast<double>(y) + 0.5) * srcHeight / dstHeight) - 0.5;
        int y0 = static_cast<int>(std::floor(srcY));
        int y1 = y0 + 1;
        double fy = srcY - y0;
        y0 = std::clamp(y0, 0, srcHeight - 1);
        y1 = std::clamp(y1, 0, srcHeight - 1);

        for (int x = 0; x < dstWidth; ++x) {
            double srcX = ((static_cast<double>(x) + 0.5) * srcWidth / dstWidth) - 0.5;
            int x0 = static_cast<int>(std::floor(srcX));
            int x1 = x0 + 1;
            double fx = srcX - x0;
            x0 = std::clamp(x0, 0, srcWidth - 1);
            x1 = std::clamp(x1, 0, srcWidth - 1);

            size_t outIndex = (static_cast<size_t>(y) * static_cast<size_t>(dstWidth) +
                               static_cast<size_t>(x)) * 4u;
            const size_t i00 = sampleIndex(x0, y0);
            const size_t i10 = sampleIndex(x1, y0);
            const size_t i01 = sampleIndex(x0, y1);
            const size_t i11 = sampleIndex(x1, y1);

            for (int c = 0; c < 4; ++c) {
                const double v00 = srcPixels[i00 + c];
                const double v10 = srcPixels[i10 + c];
                const double v01 = srcPixels[i01 + c];
                const double v11 = srcPixels[i11 + c];
                const double top = v00 + (v10 - v00) * fx;
                const double bottom = v01 + (v11 - v01) * fx;
                const double value = top + (bottom - top) * fy;
                dstPixels[outIndex + c] = static_cast<std::uint8_t>(
                    std::clamp(static_cast<int>(std::lround(value)), 0, 255));
            }
        }
    }
}

void convertPixelsRgbaToBgra(std::span<std::uint8_t> pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        std::swap(pixels[i + 0], pixels[i + 2]);
    }
}

void unpremultiplyPixels(std::span<std::uint8_t> pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        const int a = pixels[i + 3];
        if (a <= 0) {
            pixels[i + 0] = 0;
            pixels[i + 1] = 0;
            pixels[i + 2] = 0;
            continue;
        }
        if (a >= 255) {
            continue;
        }

        const float scale = 255.0f / static_cast<float>(a);
        pixels[i + 0] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(pixels[i + 0] * scale)), 0, 255));
        pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(pixels[i + 1] * scale)), 0, 255));
        pixels[i + 2] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(pixels[i + 2] * scale)), 0, 255));
    }
}

void convertPixelsToGrayscaleBgra(std::span<std::uint8_t> pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        const float b = static_cast<float>(pixels[i + 0]);
        const float g = static_cast<float>(pixels[i + 1]);
        const float r = static_cast<float>(pixels[i + 2]);
        const std::uint8_t gray = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(0.114f * b + 0.587f * g + 0.299f * r)), 0, 255));
        pixels[i + 0] = gray;
        pixels[i + 1] = gray;
        pixels[i + 2] = gray;
    }
}

void convertPixelsToGrayscaleRgba(std::span<std::uint8_t> pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        const float r = static_cast<float>(pixels[i + 0]);
        const float g = static_cast<float>(pixels[i + 1]);
        const float b = static_cast<float>(pixels[i + 2]);
        const std::uint8_t gray = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(0.299f * r + 0.587f * g + 0.114f * b)), 0, 255));
        pixels[i + 0] = gray;
        pixels[i + 1] = gray;
        pixels[i + 2] = gray;
    }
}

ProcessedImage preparePreviewRgbaImage(std::span<const std::uint8_t> renderedRgbaPixels,
                                       int renderedWidth,
                                       int renderedHeight,
                                       int targetWidth,
                                       int targetHeight,
                                       bool grayscaleOutput) {
    ProcessedImage image;
    image.width = renderedWidth;
    image.height = renderedHeight;

    const size_t expectedBytes = renderedWidth > 0 && renderedHeight > 0
        ? static_cast<size_t>(renderedWidth) * static_cast<size_t>(renderedHeight) * 4u
        : 0u;
    if (expectedBytes == 0 || renderedRgbaPixels.size() < expectedBytes) {
        image.width = 0;
        image.height = 0;
        return image;
    }

    image.pixels.assign(renderedRgbaPixels.begin(), renderedRgbaPixels.begin() + expectedBytes);
    unpremultiplyPixels(image.pixels);
    if (grayscaleOutput) {
        convertPixelsToGrayscaleRgba(image.pixels);
    }

    if (targetWidth != renderedWidth || targetHeight != renderedHeight) {
        std::vector<std::uint8_t> resizedPixels;
        resizePixelsBilinear(image.pixels, renderedWidth, renderedHeight,
                             targetWidth, targetHeight, resizedPixels);
        image.pixels = std::move(resizedPixels);
        image.width = image.pixels.empty() ? 0 : targetWidth;
        image.height = image.pixels.empty() ? 0 : targetHeight;
    }

    return image;
}

ProcessedImage prepareFinalBgraImage(std::span<const std::uint8_t> renderedRgbaPixels,
                                     int renderedWidth,
                                     int renderedHeight,
                                     const ExportSettings& settings) {
    ProcessedImage image;
    image.width = renderedWidth;
    image.height = renderedHeight;

    const size_t expectedBytes = renderedWidth > 0 && renderedHeight > 0
        ? static_cast<size_t>(renderedWidth) * static_cast<size_t>(renderedHeight) * 4u
        : 0u;
    if (expectedBytes == 0 || renderedRgbaPixels.size() < expectedBytes) {
        image.width = 0;
        image.height = 0;
        return image;
    }

    image.pixels.assign(renderedRgbaPixels.begin(), renderedRgbaPixels.begin() + expectedBytes);
    const int targetWidth = clampExportDimension(settings.size.width);
    const int targetHeight = clampExportDimension(settings.size.height);
    if (targetWidth != renderedWidth || targetHeight != renderedHeight) {
        std::vector<std::uint8_t> resizedPixels;
        resizePixelsBilinear(image.pixels, renderedWidth, renderedHeight,
                             targetWidth, targetHeight, resizedPixels);
        image.pixels = std::move(resizedPixels);
        image.width = image.pixels.empty() ? 0 : targetWidth;
        image.height = image.pixels.empty() ? 0 : targetHeight;
    }

    convertPixelsRgbaToBgra(image.pixels);
    unpremultiplyPixels(image.pixels);
    if (settings.appearance.grayscaleOutput) {
        convertPixelsToGrayscaleBgra(image.pixels);
    }
    return image;
}

} // namespace XpressFormula::Infrastructure::Export
