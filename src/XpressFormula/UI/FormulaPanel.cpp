// FormulaPanel.cpp - Formula-list panel implementation.
#include "FormulaPanel.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

namespace XpressFormula::UI {

namespace {

constexpr const char* kFormulaEditorPopupId = "Formula Editor";

struct ExamplePattern {
    const char* label;
    const char* expression;
};

const char* const kSupportedFunctions[] = {
    "sin(a)", "cos(a)", "tan(a)",
    "asin(a)", "acos(a)", "atan(a)", "atan2(y, x)",
    "sinh(a)", "cosh(a)", "tanh(a)",
    "sqrt(a)", "cbrt(a)", "abs(a)", "sign(a)",
    "ceil(a)", "floor(a)", "round(a)",
    "log(a)", "log(base, value)", "log2(a)", "log10(a)", "exp(a)",
    "pow(a, b)", "min(a, b)", "max(a, b)", "mod(a, b)",
};

const ExamplePattern kExamplePatterns[] = {
    { "2D curve",       "sin(x) * exp(-x*x/12)" },
    { "2D curve (eq)",  "y = cos(x)" },
    { "3D surface",     "z = sin(x) * cos(y)" },
    { "3D surface alt", "sqrt(abs(x*y))" },
    { "3D surface alt", "sin(sqrt(x^2+y^2))" },
    { "Implicit 2D",    "x^2 + y^2 = 100" },
    { "Implicit 2D",    "pow(x,2)/25 + pow(y,2)/9 = 1" },
    { "Scalar field",   "x^2 + y^2 + z^2 = 16" },
    { "Implicit 3D",    "(x^2+y^2+z^2+21)^2 - 100*(x^2+y^2) = 0" },
    { "Implicit 3D",    "(x^2+y^2+z^2+5)^2 - 36*(x^2+y^2) = 0" },
    { "Implicit 3D",    "pow(abs(pow(pow(abs(x),4)+pow(abs(y),4),0.25)-1.0),4)+pow(abs(z),4)=pow(0.35,4)" },
    { "Implicit 3D",    "max(pow(pow(abs(x/1.25),6)+pow(abs(y/1.00),6)+pow(abs(z/0.82),6),1.0/6)-1,1-sqrt(pow(abs(y)/(0.28+0.17*pow(abs(x/1.25),4)),2)+pow(abs(z)/(0.24+0.15*pow(abs(x/1.25),4)),2)))=0" },
    { "Implicit 3D",    "max(pow(pow(abs(x/1.18),8)+pow(abs(y/1.02),8)+pow(abs(z/0.88),8),1.0/8)-1,-min(sqrt(y^2+z^2)-(0.22+0.20*pow(abs(x/1.18),4)),min(sqrt(x^2+z^2)-(0.22+0.20*pow(abs(y/1.02),4)),sqrt(x^2+y^2)-(0.22+0.20*pow(abs(z/0.88),4)))))=0" },
    { "Scalar field",   "sin(x) + cos(y) + z = 0" },
};

void loadEditorText(char* dest, size_t destSize, const char* value) {
    if (!dest || destSize == 0) {
        return;
    }
    strncpy_s(dest, destSize, value ? value : "", _TRUNCATE);
}

} // namespace

void FormulaPanel::openEditor(const FormulaEntry& formula, int formulaIndex) {
    m_editorFormulaIndex = formulaIndex;
    loadEditorText(m_editorBuffer, sizeof(m_editorBuffer), formula.inputBuffer);
    m_openEditorPopupNextFrame = true;
    m_focusEditorInput = true;
}

void FormulaPanel::renderEditorDialog(std::vector<FormulaEntry>& formulas) {
    if (m_openEditorPopupNextFrame) {
        ImGui::OpenPopup(kFormulaEditorPopupId);
        m_openEditorPopupNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(920.0f, 610.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(kFormulaEditorPopupId, nullptr,
                               ImGuiWindowFlags_NoSavedSettings)) {
        if (m_editorFormulaIndex < 0 || m_editorFormulaIndex >= static_cast<int>(formulas.size())) {
            ImGui::TextWrapped("The selected formula is no longer available.");
            if (ImGui::Button("Close")) {
                m_editorFormulaIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        FormulaEntry& formula = formulas[m_editorFormulaIndex];

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 9.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

        ImGui::Text("Formula %d", m_editorFormulaIndex + 1);
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", formula.typeLabel());
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::TextWrapped("Enter an expression or equation. Supported forms include y=f(x), "
                           "z=f(x,y), F(x,y)=0, and F(x,y,z)=0.");
        ImGui::TextWrapped("Variables: x, y, z    Constants: pi, e, tau");
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        if (m_focusEditorInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusEditorInput = false;
        }

        const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 7.0f;
        ImGui::InputTextMultiline("##formula_editor", m_editorBuffer, sizeof(m_editorBuffer),
                                  ImVec2(-1.0f, editorHeight));

        const size_t editorLength = strnlen_s(m_editorBuffer, sizeof(m_editorBuffer));
        FormulaEntry editorPreview;
        strncpy_s(editorPreview.inputBuffer, sizeof(editorPreview.inputBuffer), m_editorBuffer, _TRUNCATE);
        editorPreview.parse();

        ImGui::TextDisabled("Editor buffer: %zu / %zu", editorLength, sizeof(m_editorBuffer) - 1);
        if (editorLength >= sizeof(formula.inputBuffer)) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
                               "Will be truncated to %zu chars when applied",
                               sizeof(formula.inputBuffer) - 1);
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::BeginChild("FormulaEditorValidation", ImVec2(0.0f, 80.0f), true);
        ImGui::TextUnformatted("Live validation");
        if (editorPreview.lastParsedText.empty()) {
            ImGui::TextDisabled("Start typing to validate the formula syntax and detected plot type.");
        } else if (editorPreview.isValid()) {
            ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f),
                               "Valid (%s)", editorPreview.typeLabel());
            ImGui::TextDisabled("Detected variables: %d   %s",
                                editorPreview.variableCount,
                                editorPreview.isEquation ? "Equation" : "Expression");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Invalid");
            if (!editorPreview.error.empty()) {
                ImGui::TextWrapped("%s", editorPreview.error.c_str());
            } else {
                ImGui::TextWrapped("Unable to parse the current formula.");
            }
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f))) {
            strncpy_s(formula.inputBuffer, sizeof(formula.inputBuffer), m_editorBuffer, _TRUNCATE);
            formula.parse();
            m_editorFormulaIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            m_editorFormulaIndex = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::TextUnformatted("Reference");
        ImGui::TextDisabled("Functions and examples are shown side by side to keep the dialog compact.");

        const ImGuiStyle& style = ImGui::GetStyle();
        const float refsHeight = 175.0f;
        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float leftWidth = std::max(220.0f, (totalWidth - style.ItemSpacing.x) * 0.45f);

        ImGui::BeginChild("FormulaEditorFunctions", ImVec2(leftWidth, refsHeight), true);
        ImGui::TextUnformatted("Supported functions");
        ImGui::Separator();
        for (const char* fn : kSupportedFunctions) {
            ImGui::BulletText("%s", fn);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("FormulaEditorExamples", ImVec2(0.0f, refsHeight), true);
        ImGui::TextUnformatted("Example patterns");
        ImGui::TextDisabled("Click row or Load");
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(sizeof(kExamplePatterns) / sizeof(kExamplePatterns[0])); ++i) {
            const ExamplePattern& example = kExamplePatterns[i];
            ImGui::PushID(i);
            if (ImGui::SmallButton("Load")) {
                loadEditorText(m_editorBuffer, sizeof(m_editorBuffer), example.expression);
                m_focusEditorInput = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", example.label);
            ImGui::SameLine();
            if (ImGui::Selectable(example.expression, false)) {
                loadEditorText(m_editorBuffer, sizeof(m_editorBuffer), example.expression);
                m_focusEditorInput = true;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen(kFormulaEditorPopupId)) {
        m_editorFormulaIndex = -1;
        m_focusEditorInput = false;
    }
}

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
        ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 150.0f));
        if (ImGui::InputText("##expr", f.inputBuffer, sizeof(f.inputBuffer))) {
            f.parse();
        }
        ImGui::SameLine();

        if (ImGui::SmallButton("Edit")) {
            openEditor(f, i);
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
            if (f.isEquation) {
                ImGui::SliderFloat("z slice / center", &f.zSlice, -10.0f, 10.0f, "z = %.2f");
            } else {
                ImGui::SliderFloat("z slice", &f.zSlice, -10.0f, 10.0f, "z = %.2f");
            }
        }

        ImGui::PopID();
    }

    if (removeIndex >= 0) {
        if (m_editorFormulaIndex == removeIndex) {
            m_editorFormulaIndex = -1;
        } else if (m_editorFormulaIndex > removeIndex) {
            --m_editorFormulaIndex;
        }
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

    renderEditorDialog(formulas);
}

} // namespace XpressFormula::UI
