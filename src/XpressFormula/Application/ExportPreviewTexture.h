// SPDX-License-Identifier: MIT
// ExportPreviewTexture.h - D3D preview texture resource owner.
#pragma once

#include "../Platform/Windows/ComPtr.h"

#include <cstdint>
#include <d3d11.h>
#include <span>
#include <string>

namespace XpressFormula::Application {

class ExportPreviewTexture {
public:
    ExportPreviewTexture() = default;
    ExportPreviewTexture(const ExportPreviewTexture&) = delete;
    ExportPreviewTexture& operator=(const ExportPreviewTexture&) = delete;

    void reset() noexcept;

    [[nodiscard]] bool update(ID3D11Device* device,
                              ID3D11DeviceContext* deviceContext,
                              std::span<const std::uint8_t> rgbaPixels,
                              int width,
                              int height,
                              std::string& error);

    [[nodiscard]] ID3D11ShaderResourceView* srv() const noexcept { return m_srv.get(); }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] bool hasTexture() const noexcept {
        return m_srv && m_texture && m_width > 0 && m_height > 0;
    }

private:
    Platform::Windows::ComPtr<ID3D11Texture2D> m_texture;
    Platform::Windows::ComPtr<ID3D11ShaderResourceView> m_srv;
    int m_width = 0;
    int m_height = 0;
};

} // namespace XpressFormula::Application
