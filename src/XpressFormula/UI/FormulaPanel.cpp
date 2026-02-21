// FormulaPanel.cpp - Formula-list panel implementation.
#include "FormulaPanel.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

namespace XpressFormula::UI {

void FormulaPanel::render(std::vector<FormulaEntry>& formulas) {
    ImGui::TextUnformatted("Formulas");
    ImGui::Separator();

    // --- Preset examples ---
    if (ImGui::CollapsingHeader("Presets")) {
        static const char* presets[] = {
            "sin(x)",
            "x^2",
            "cos(x) * exp(-x*x/10)",
            "sqrt(abs(x))",
            "x^2 + y^2",
            "z = sin(x) * cos(y)",
            "sin(x) * cos(y)",
            "x^2 + y^2 = 100",
            "x^2 + y^2 + z^2 - 4",
        };
        for (const char* p : presets) {
            if (ImGui::SmallButton(p)) {
                FormulaEntry entry;
                strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), p, _TRUNCATE);
                int idx = m_nextColorIndex % kPaletteSize;
                std::memcpy(entry.color, kDefaultPalette[idx], sizeof(entry.color));
                m_nextColorIndex++;
                entry.parse();
                formulas.push_back(std::move(entry));
            }
        }
    }

    ImGui::Separator();
    ImGui::TextWrapped("Enter expressions like y=f(x), z=f(x,y), or equations like x^2+y^2=100.");

    // --- Formula list ---
    int removeIndex = -1;
    for (int i = 0; i < static_cast<int>(formulas.size()); ++i) {
        ImGui::PushID(i);
        FormulaEntry& f = formulas[i];

        // Visibility toggle
        ImGui::Checkbox("##vis", &f.visible);
        ImGui::SameLine();

        // Color picker
        ImGui::ColorEdit4("##col", f.color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::SameLine();

        // Input field
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
        if (ImGui::InputText("##expr", f.inputBuffer, sizeof(f.inputBuffer))) {
            f.parse();
        }
        ImGui::SameLine();

        // Type label
        ImGui::TextUnformatted(f.typeLabel());
        ImGui::SameLine();

        // Delete button
        if (ImGui::SmallButton("X")) {
            removeIndex = i;
        }

        // Error message
        if (!f.error.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
            ImGui::TextWrapped("  Error: %s", f.error.c_str());
            ImGui::PopStyleColor();
        }

        // Z-slice slider for scalar fields that include z.
        if (f.renderKind == FormulaRenderKind::ScalarField3D && f.isValid()) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderFloat("z slice", &f.zSlice, -10.0f, 10.0f, "z = %.2f");
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0) {
        formulas.erase(formulas.begin() + removeIndex);
    }

    ImGui::Separator();

    // --- Add button ---
    if (ImGui::Button("+ Add Formula", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        FormulaEntry entry;
        int idx = m_nextColorIndex % kPaletteSize;
        std::memcpy(entry.color, kDefaultPalette[idx], sizeof(entry.color));
        m_nextColorIndex++;
        formulas.push_back(std::move(entry));
    }
}

} // namespace XpressFormula::UI
