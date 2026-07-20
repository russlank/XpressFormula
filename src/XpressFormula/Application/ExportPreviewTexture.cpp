// SPDX-License-Identifier: MIT
// ExportPreviewTexture.cpp - D3D preview texture resource owner.
#include "ExportPreviewTexture.h"

namespace XpressFormula::Application {

void ExportPreviewTexture::reset() noexcept {
    m_srv.reset();
    m_texture.reset();
    m_width = 0;
    m_height = 0;
}

bool ExportPreviewTexture::update(ID3D11Device* device,
                                  ID3D11DeviceContext* deviceContext,
                                  std::span<const std::uint8_t> rgbaPixels,
                                  int width,
                                  int height,
                                  std::string& error) {
    error.clear();
    if (!device || !deviceContext) {
        error = "Preview unavailable: renderer not initialized.";
        return false;
    }
    if (width <= 0 || height <= 0 ||
        rgbaPixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4u) {
        error = "Preview render produced invalid size.";
        reset();
        return false;
    }

    const bool recreateTexture = (!m_texture || !m_srv || m_width != width || m_height != height);
    if (recreateTexture) {
        reset();

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = static_cast<UINT>(width);
        texDesc.Height = static_cast<UINT>(height);
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = rgbaPixels.data();
        initData.SysMemPitch = static_cast<UINT>(width * 4);

        if (FAILED(device->CreateTexture2D(&texDesc, &initData, m_texture.put())) || !m_texture) {
            error = "Preview texture creation failed.";
            reset();
            return false;
        }

        if (FAILED(device->CreateShaderResourceView(m_texture.get(), nullptr, m_srv.put())) ||
            !m_srv) {
            error = "Preview texture view creation failed.";
            reset();
            return false;
        }
        m_width = width;
        m_height = height;
    } else {
        deviceContext->UpdateSubresource(m_texture.get(),
                                         0,
                                         nullptr,
                                         rgbaPixels.data(),
                                         static_cast<UINT>(width * 4),
                                         0);
    }

    return true;
}

} // namespace XpressFormula::Application
