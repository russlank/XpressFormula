// ResponsiveRows.h - ImGui child region with deterministic row placement.
#pragma once

#include "imgui.h"

namespace XpressFormula::UI::UiKit {

struct ResponsiveRowsOptions {
    ImVec2 padding = ImVec2(6.0f, 4.0f);
    bool bordered = false;
    bool noScrollbar = true;
    float extraBottomPadding = 2.0f;
};

class ResponsiveRows {
public:
    ResponsiveRows(const char* id,
                   int rowCount,
                   const ResponsiveRowsOptions& options = {});
    ~ResponsiveRows();

    bool begin();
    void beginRow(int rowIndex);

    float height() const { return m_height; }
    float rowStride() const { return m_rowStride; }

    ResponsiveRows(const ResponsiveRows&) = delete;
    ResponsiveRows& operator=(const ResponsiveRows&) = delete;
    ResponsiveRows(ResponsiveRows&&) = delete;
    ResponsiveRows& operator=(ResponsiveRows&&) = delete;

private:
    const char* m_id = nullptr;
    int m_rowCount = 0;
    float m_height = 0.0f;
    float m_rowStride = 0.0f;
    ImVec2 m_padding{};
    bool m_bordered = false;
    bool m_noScrollbar = true;
    bool m_begun = false;
    bool m_open = false;
};

} // namespace XpressFormula::UI::UiKit
