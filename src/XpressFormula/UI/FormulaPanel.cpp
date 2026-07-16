// FormulaPanel.cpp - Formula-list panel implementation.
#include "FormulaPanel.h"
#include "Components/FormulaCard.h"
#include "FormulaExamples.h"
#include "FormulaListActions.h"
#include "UiKit/ResponsiveLayout.h"
#include "UiKit/UiMetrics.h"
#include "UiKit/UiScopes.h"
#include "../Core/FunctionRegistry.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace XpressFormula::UI {

namespace {

constexpr const char* kFormulaEditorPopupId = "Formula Editor";
constexpr const char* kFunctionHelpPopupId = "Function Details";
constexpr const char* kExampleHelpPopupId = "Example Details";
constexpr const char* kDeleteFormulaPopupId = "Delete Formula";

void loadEditorText(char* dest, size_t destSize, const char* value) {
    if (!dest || destSize == 0) {
        return;
    }
    strncpy_s(dest, destSize, value ? value : "", _TRUNCATE);
}

void showWrappedTooltip(const char* first, const char* second = nullptr, const char* third = nullptr) {
    ImGui::BeginTooltip();
    {
        UiKit::TextWrapScope wrap(ImGui::GetFontSize() * UiKit::metrics().tooltipWrapEm);
        if (first && first[0] != '\0') {
            ImGui::TextUnformatted(first);
        }
        if (second && second[0] != '\0') {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", second);
        }
        if (third && third[0] != '\0') {
            ImGui::Spacing();
            ImGui::TextUnformatted(third);
        }
    }
    ImGui::EndTooltip();
}

void wrappedBlock(const char* id, const char* text, float height) {
    ImGui::BeginChild(id, ImVec2(0.0f, height), true);
    {
        UiKit::TextWrapScope wrap(ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(text ? text : "");
    }
    ImGui::EndChild();
}

bool canCopyEquivalent(const Core::FunctionInfo& fn) {
    return fn.equivalentFormula != nullptr &&
           fn.equivalentFormula[0] != '\0' &&
           std::string_view(fn.equivalentFormula).find("not expressible") == std::string_view::npos;
}

} // namespace

void FormulaPanel::openEditor(const FormulaEntry& formula, int formulaIndex) {
    m_editorFormulaIndex = formulaIndex;
    loadEditorText(m_editorBuffer, sizeof(m_editorBuffer), formula.inputBuffer);
    m_openEditorPopupNextFrame = true;
    m_focusEditorInput = true;
    // Reset the cached preview so it re-parses on the first frame.
    m_editorPreviousText.clear();
    m_editorPreview = FormulaEntry{};
}

void FormulaPanel::loadEditorFormula(const char* expression) {
    loadEditorText(m_editorBuffer, sizeof(m_editorBuffer), expression);
    m_focusEditorInput = true;
    m_editorPreviousText.clear();
    m_editorPreview = FormulaEntry{};
}

void FormulaPanel::requestDeleteFormula(int formulaIndex) {
    m_pendingDeleteFormulaIndex = formulaIndex;
    m_openDeleteConfirmPopupNextFrame = true;
}

void FormulaPanel::renderDeleteConfirmationDialog(std::vector<FormulaEntry>& formulas) {
    if (m_openDeleteConfirmPopupNextFrame) {
        ImGui::OpenPopup(kDeleteFormulaPopupId);
        m_openDeleteConfirmPopupNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(kDeleteFormulaPopupId, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        const bool validIndex = FormulaListActions::isValidFormulaIndex(
            formulas, m_pendingDeleteFormulaIndex);
        if (!validIndex) {
            ImGui::TextWrapped("The selected formula is no longer available.");
            if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
                m_pendingDeleteFormulaIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        const FormulaEntry& formula = formulas[static_cast<std::size_t>(m_pendingDeleteFormulaIndex)];
        ImGui::Text("Delete Formula %d?", m_pendingDeleteFormulaIndex + 1);
        ImGui::Spacing();
        {
            UiKit::TextWrapScope wrap(ImGui::GetFontSize() * UiKit::metrics().tooltipWrapEm);
            ImGui::TextWrapped("%s", FormulaListActions::formulaExpression(formula).c_str());
        }
        ImGui::Spacing();
        ImGui::TextDisabled("This only removes the formula from the current list.");
        ImGui::Separator();

        {
            UiKit::StyleColorScope buttonColor(
                ImGuiCol_Button, ImVec4(0.70f, 0.16f, 0.16f, 1.0f));
            UiKit::StyleColorScope hoveredColor(
                ImGuiCol_ButtonHovered, ImVec4(0.86f, 0.22f, 0.22f, 1.0f));
            if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
                FormulaListActions::deleteFormula(formulas, m_pendingDeleteFormulaIndex,
                                                  &m_editorFormulaIndex);
                m_pendingDeleteFormulaIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            m_pendingDeleteFormulaIndex = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen(kDeleteFormulaPopupId) && !m_openDeleteConfirmPopupNextFrame) {
        m_pendingDeleteFormulaIndex = -1;
    }
}

void FormulaPanel::renderFunctionHelpDialog() {
    if (m_openFunctionHelpPopupNextFrame) {
        ImGui::OpenPopup(kFunctionHelpPopupId);
        m_openFunctionHelpPopupNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520.0f, 420.0f), ImVec2(900.0f, 760.0f));
    if (ImGui::BeginPopupModal(kFunctionHelpPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        const Core::FunctionInfo* fn = m_selectedFunctionHelp;
        if (fn == nullptr) {
            ImGui::TextWrapped("No function is selected.");
            if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Function: %s", fn->name);
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", fn->category);
        ImGui::Separator();

        ImGui::TextUnformatted("Signature");
        wrappedBlock("FunctionSignatureBlock", fn->signature, ImGui::GetTextLineHeightWithSpacing() * 2.0f);
        ImGui::TextUnformatted("Description");
        ImGui::TextWrapped("%s", fn->detailedDescription);
        ImGui::Spacing();

        ImGui::TextUnformatted("Equivalent formula / note");
        wrappedBlock("FunctionEquivalentBlock", fn->equivalentFormula,
                     ImGui::GetTextLineHeightWithSpacing() * 3.0f);
        ImGui::TextUnformatted("Example");
        wrappedBlock("FunctionExampleBlock", fn->example,
                     ImGui::GetTextLineHeightWithSpacing() * 4.0f);

        ImGui::Separator();
        const bool copyEquivalent = canCopyEquivalent(*fn);
        {
            UiKit::DisabledScope disabled(!copyEquivalent);
            if (ImGui::Button("Copy equivalent")) {
                ImGui::SetClipboardText(fn->equivalentFormula);
            }
        }
        if (!copyEquivalent && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            showWrappedTooltip("This entry is an explanatory note rather than a reusable formula.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy example")) {
            ImGui::SetClipboardText(fn->example);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load example")) {
            loadEditorFormula(fn->example);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FormulaPanel::renderExampleHelpDialog() {
    if (m_openExampleHelpPopupNextFrame) {
        ImGui::OpenPopup(kExampleHelpPopupId);
        m_openExampleHelpPopupNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(680.0f, 420.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520.0f, 360.0f), ImVec2(900.0f, 700.0f));
    if (ImGui::BeginPopupModal(kExampleHelpPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        const auto examples = examplePatterns();
        const bool validIndex = m_selectedExampleHelpIndex >= 0 &&
            m_selectedExampleHelpIndex < static_cast<int>(examples.size());

        if (!validIndex) {
            ImGui::TextWrapped("No example is selected.");
            if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        const ExamplePattern& example = examples[m_selectedExampleHelpIndex];
        ImGui::Text("Example: %s", example.label);
        ImGui::Separator();
        ImGui::TextUnformatted("Description");
        ImGui::TextWrapped("%s", example.description);
        ImGui::Spacing();
        ImGui::TextUnformatted("Formula");
        wrappedBlock("ExampleFormulaBlock", example.expression,
                     ImGui::GetTextLineHeightWithSpacing() * 7.0f);

        ImGui::Separator();
        if (ImGui::Button("Load", ImVec2(120.0f, 0.0f))) {
            loadEditorFormula(example.expression);
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy formula")) {
            ImGui::SetClipboardText(example.expression);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FormulaPanel::renderEditorDialog(std::vector<FormulaEntry>& formulas) {
    if (m_openEditorPopupNextFrame) {
        ImGui::OpenPopup(kFormulaEditorPopupId);
        m_openEditorPopupNextFrame = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImVec2(920.0f, 650.0f);
    const UiKit::ModalSizePlan modalSize = UiKit::planModalSize({
        { workSize.x, workSize.y },
        { 0.84f, 0.84f },
        { 860.0f, 620.0f },
        { workSize.x * 0.02f, workSize.y * 0.02f }
    });

    ImGui::SetNextWindowSize(ImVec2(modalSize.size.x, modalSize.size.y),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(modalSize.minimumSize.x, modalSize.minimumSize.y),
        ImVec2(modalSize.maximumSize.x, modalSize.maximumSize.y));
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

        UiKit::StyleVarScope itemSpacing(
            ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 9.0f));
        UiKit::StyleVarScope framePadding(
            ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

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

        // Only re-parse when the editor text actually changes (avoids a full tokenize+parse+AST
        // allocation every frame while the editor is open).
        std::string currentEditorText(m_editorBuffer, editorLength);
        if (currentEditorText != m_editorPreviousText) {
            m_editorPreviousText = currentEditorText;
            m_editorPreview = FormulaEntry{};
            strncpy_s(m_editorPreview.inputBuffer, sizeof(m_editorPreview.inputBuffer), m_editorBuffer, _TRUNCATE);
            m_editorPreview.parse();
        }
        const FormulaEntry& editorPreview = m_editorPreview;

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
        ImGui::TextDisabled("Browse functions and examples, then copy or load formulas into the editor.");

        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float referenceHeight = std::max(lineHeight * 10.0f,
                                               ImGui::GetContentRegionAvail().y - lineHeight * 1.5f);

        if (ImGui::BeginTabBar("FormulaEditorReferenceTabs")) {
            if (ImGui::BeginTabItem("Functions")) {
                ImGui::BeginChild("FormulaEditorFunctionsTab", ImVec2(0.0f, referenceHeight), true);
                const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp;

                if (ImGui::BeginTable("FormulaEditorFunctionTable", 4, flags)) {
                    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 145.0f);
                    ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 95.0f);
                    ImGui::TableHeadersRow();

                    for (const Core::FunctionInfo& fn : Core::functionRegistry()) {
                        UiKit::IdScope functionId(fn.name);
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("%s", fn.category);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(fn.signature);
                        if (ImGui::IsItemHovered()) {
                            showWrappedTooltip(fn.signature, fn.detailedDescription, fn.example);
                        }

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextWrapped("%s", fn.description);
                        if (ImGui::IsItemHovered()) {
                            showWrappedTooltip(fn.description, fn.detailedDescription);
                        }

                        ImGui::TableSetColumnIndex(3);
                        if (ImGui::SmallButton("Details")) {
                            m_selectedFunctionHelp = &fn;
                            m_openFunctionHelpPopupNextFrame = true;
                        }
                        if (ImGui::IsItemHovered()) {
                            showWrappedTooltip("Open detailed function help.");
                        }

                    }
                    ImGui::EndTable();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Examples")) {
                ImGui::BeginChild("FormulaEditorExamplesTab", ImVec2(0.0f, referenceHeight), true);
                const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp;

                if (ImGui::BeginTable("FormulaEditorExampleTable", 3, flags)) {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableHeadersRow();

                    const auto examples = examplePatterns();
                    for (int i = 0; i < static_cast<int>(examples.size()); ++i) {
                        const ExamplePattern& example = examples[i];
                        UiKit::IdScope exampleId(i);
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(example.label);
                        if (ImGui::IsItemHovered()) {
                            showWrappedTooltip(example.label, example.description, example.expression);
                        }

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextWrapped("%s", example.description);
                        if (ImGui::IsItemHovered()) {
                            showWrappedTooltip(example.description, nullptr, example.expression);
                        }

                        ImGui::TableSetColumnIndex(2);
                        if (ImGui::SmallButton("Load")) {
                            loadEditorFormula(example.expression);
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Copy")) {
                            ImGui::SetClipboardText(example.expression);
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Details")) {
                            m_selectedExampleHelpIndex = i;
                            m_openExampleHelpPopupNextFrame = true;
                        }

                    }
                    ImGui::EndTable();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        renderFunctionHelpDialog();
        renderExampleHelpDialog();

        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen(kFormulaEditorPopupId)) {
        m_editorFormulaIndex = -1;
        m_focusEditorInput = false;
        m_selectedFunctionHelp = nullptr;
        m_selectedExampleHelpIndex = -1;
    }
}

void FormulaPanel::render(std::vector<FormulaEntry>& formulas) {
    ImGui::TextUnformatted("Formulas");
    ImGui::Separator();

    if (ImGui::Button("+ Add Formula", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        FormulaEntry entry;
        int idx = m_nextColorIndex % kPaletteSize;
        std::memcpy(entry.color, kDefaultPalette[idx], sizeof(entry.color));
        m_nextColorIndex++;
        formulas.push_back(std::move(entry));
        openEditor(formulas.back(), static_cast<int>(formulas.size()) - 1);
    }

    ImGui::Spacing();
    ImGui::TextWrapped("Enter expressions like y=f(x), z=f(x,y), or equations like x^2+y^2=100.");

    // --- Preset examples ---
    if (ImGui::CollapsingHeader("Presets")) {
        const auto examples = examplePatterns();
        for (int i = 0; i < static_cast<int>(examples.size()); ++i) {
            const ExamplePattern& preset = examples[i];
            if (!preset.includeInPresets) {
                continue;
            }

            UiKit::IdScope presetId(i);
            if (ImGui::SmallButton(preset.label)) {
                FormulaEntry entry;
                strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), preset.expression, _TRUNCATE);
                int idx = m_nextColorIndex % kPaletteSize;
                std::memcpy(entry.color, kDefaultPalette[idx], sizeof(entry.color));
                m_nextColorIndex++;
                entry.parse();
                formulas.push_back(std::move(entry));
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                {
                    UiKit::TextWrapScope wrap(
                        ImGui::GetFontSize() * UiKit::metrics().tooltipWrapEm);
                    ImGui::TextUnformatted(preset.label);
                    ImGui::Spacing();
                    ImGui::TextWrapped("%s", preset.description);
                    ImGui::Spacing();
                    ImGui::TextUnformatted(preset.expression);
                }
                ImGui::EndTooltip();
            }
        }
    }

    ImGui::Separator();

    // --- Formula list ---
    int duplicateIndex = -1;
    int hideOthersIndex = -1;
    int moveFromIndex = -1;
    int moveToIndex = -1;

    if (formulas.empty()) {
        ImGui::TextDisabled("No formulas yet.");
    }

    for (int i = 0; i < static_cast<int>(formulas.size()); ++i) {
        FormulaEntry& formula = formulas[static_cast<std::size_t>(i)];
        const Components::FormulaCardAction action = Components::renderFormulaCard(
            formula,
            Components::FormulaCardContext{
                i,
                static_cast<int>(formulas.size()),
                ImGui::GetContentRegionAvail().x
            });

        switch (action.type) {
            case Components::FormulaCardActionType::Edit:
                openEditor(formula, i);
                break;
            case Components::FormulaCardActionType::Duplicate:
                duplicateIndex = action.formulaIndex;
                break;
            case Components::FormulaCardActionType::Copy: {
                const std::string expression = FormulaListActions::formulaExpression(formula);
                ImGui::SetClipboardText(expression.c_str());
                break;
            }
            case Components::FormulaCardActionType::HideOthers:
                hideOthersIndex = action.formulaIndex;
                break;
            case Components::FormulaCardActionType::MoveUp:
                moveFromIndex = action.formulaIndex;
                moveToIndex = action.formulaIndex - 1;
                break;
            case Components::FormulaCardActionType::MoveDown:
                moveFromIndex = action.formulaIndex;
                moveToIndex = action.formulaIndex + 1;
                break;
            case Components::FormulaCardActionType::RequestDelete:
                requestDeleteFormula(action.formulaIndex);
                break;
            case Components::FormulaCardActionType::None:
            default:
                break;
        }
    }

    if (hideOthersIndex >= 0) {
        FormulaListActions::hideOtherFormulas(formulas, hideOthersIndex);
    }
    if (duplicateIndex >= 0) {
        FormulaListActions::duplicateFormula(formulas, duplicateIndex, &m_editorFormulaIndex);
    }
    if (moveFromIndex >= 0 && moveToIndex >= 0) {
        FormulaListActions::moveFormula(formulas, moveFromIndex, moveToIndex,
                                        &m_editorFormulaIndex);
    }

    renderDeleteConfirmationDialog(formulas);
    renderEditorDialog(formulas);
}

} // namespace XpressFormula::UI
