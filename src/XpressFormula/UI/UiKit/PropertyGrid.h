// PropertyGrid.h - Reusable three-column property table pattern.
#pragma once

namespace XpressFormula::UI::UiKit {

struct PropertyGridOptions {
    float labelWidth = 122.0f;
    float resetWidth = 58.0f;
    bool showResetColumn = true;
};

class PropertyGrid {
public:
    PropertyGrid(const char* id, const PropertyGridOptions& options = {});
    ~PropertyGrid();

    bool begin();

    bool sliderFloat(const char* label,
                     float& value,
                     float minimum,
                     float maximum,
                     float defaultValue,
                     const char* format,
                     const char* tooltip = nullptr);

    bool sliderInt(const char* label,
                   int& value,
                   int minimum,
                   int maximum,
                   int defaultValue,
                   const char* tooltip = nullptr);

    PropertyGrid(const PropertyGrid&) = delete;
    PropertyGrid& operator=(const PropertyGrid&) = delete;
    PropertyGrid(PropertyGrid&&) = delete;
    PropertyGrid& operator=(PropertyGrid&&) = delete;

private:
    bool resetButton(const char* label);
    void drawLabel(const char* label, const char* tooltip);

    const char* m_id = nullptr;
    PropertyGridOptions m_options;
    bool m_open = false;
};

} // namespace XpressFormula::UI::UiKit
