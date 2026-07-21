// FormulaPanel.cpp - Formula-list panel implementation.
#include "FormulaPanel.h"
#include "Components/FormulaCard.h"
#include "FormulaExamples.h"
#include "FormulaListActions.h"
#include "FormulaPresentation.h"
#include "UiKit/ResponsiveLayout.h"
#include "UiKit/UiMetrics.h"
#include "UiKit/UiScopes.h"
#include "../Core/ConstantRegistry.h"
#include "../Core/FunctionRegistry.h"
#include "imgui.h"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace XpressFormula::UI {

namespace {

constexpr const char* kFormulaEditorPopupId = "Formula Editor";
constexpr const char* kFunctionHelpPopupId = "Function Details";
constexpr const char* kExampleHelpPopupId = "Example Details";
constexpr const char* kDeleteFormulaPopupId = "Delete Formula";

void applyPaletteColor(Model::Formula& formula, int paletteIndex) {
    for (std::size_t channel = 0; channel < formula.color.size(); ++channel) {
        formula.color[channel] = kDefaultPalette[paletteIndex][channel];
    }
}

int inputTextResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) {
        return 0;
    }

    auto* text = static_cast<std::string*>(data->UserData);
    text->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = text->data();
    return 0;
}

bool inputTextMultilineString(const char* label, std::string& value, const ImVec2& size) {
    if (value.capacity() == 0) {
        value.reserve(256);
    }
    return ImGui::InputTextMultiline(label,
                                     value.data(),
                                     value.capacity() + 1,
                                     size,
                                     ImGuiInputTextFlags_CallbackResize,
                                     inputTextResizeCallback,
                                     &value);
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

const std::string& constantNamesText() {
    static const std::string text = [] {
        std::string value = "Constants: ";
        bool first = true;
        for (const Core::ConstantInfo& constant : Core::constantRegistry()) {
            if (!first) {
                value += ", ";
            }
            value += constant.name;
            first = false;
        }
        return value;
    }();
    return text;
}

} // namespace

void FormulaPanel::openAddEditor(Model::Formula formula) {
    m_editorState.openAdd(std::move(formula));
    m_openEditorPopupNextFrame = true;
    m_focusEditorInput = true;
}

void FormulaPanel::openEditor(const Model::Formula& formula) {
    m_editorState.open(formula);
    m_openEditorPopupNextFrame = true;
    m_focusEditorInput = true;
}

void FormulaPanel::loadEditorFormula(const char* expression) {
    m_editorState.loadText(expression ? expression : "");
    m_focusEditorInput = true;
}

void FormulaPanel::requestDeleteFormula(Model::FormulaId formulaId) {
    m_pendingDeleteFormulaId = formulaId;
    m_openDeleteConfirmPopupNextFrame = true;
}

void FormulaPanel::renderDeleteConfirmationDialog(std::span<const Model::Formula> formulas,
                                                  FormulaPanelActions& actions) {
    if (m_openDeleteConfirmPopupNextFrame) {
        ImGui::OpenPopup(kDeleteFormulaPopupId);
        m_openDeleteConfirmPopupNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(kDeleteFormulaPopupId, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings)) {
        const std::optional<std::size_t> formulaIndex =
            m_pendingDeleteFormulaId
                ? FormulaListActions::findFormulaIndex(formulas, *m_pendingDeleteFormulaId)
                : std::nullopt;
        if (!formulaIndex.has_value()) {
            ImGui::TextWrapped("The selected formula is no longer available.");
            if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
                m_pendingDeleteFormulaId.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        const Model::Formula& formula = formulas[*formulaIndex];
        ImGui::Text("Delete Formula %d?", static_cast<int>(*formulaIndex + 1));
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
                actions.commands.emplace_back(RemoveFormulaCommand{ *m_pendingDeleteFormulaId });
                if (m_editorState.targetId == m_pendingDeleteFormulaId) {
                    m_editorState.close();
                }
                m_pendingDeleteFormulaId.reset();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            m_pendingDeleteFormulaId.reset();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen(kDeleteFormulaPopupId) && !m_openDeleteConfirmPopupNextFrame) {
        m_pendingDeleteFormulaId.reset();
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

void FormulaPanel::renderEditorDialog(std::span<const Model::Formula> formulas,
                                      FormulaPanelActions& actions) {
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
        if (!m_editorState.active()) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const bool addMode = m_editorState.adding();
        const std::optional<std::size_t> formulaIndex =
            (!addMode && m_editorState.targetId)
                ? FormulaListActions::findFormulaIndex(formulas, *m_editorState.targetId)
                : std::nullopt;
        if (!addMode && !formulaIndex.has_value()) {
            ImGui::TextWrapped("The selected formula is no longer available.");
            if (ImGui::Button("Close")) {
                m_editorState.close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }

        const Model::Formula* formula = addMode ? nullptr : &formulas[*formulaIndex];

        {
        UiKit::StyleVarScope itemSpacing(
            ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 9.0f));
        UiKit::StyleVarScope framePadding(
            ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

        if (addMode) {
            ImGui::TextUnformatted("Add Formula");
        } else {
            ImGui::Text("Formula %d", static_cast<int>(*formulaIndex + 1));
        }
        ImGui::SameLine();
        if (m_editorState.previewAvailable && m_editorState.preview.isValid()) {
            ImGui::TextDisabled("(%s)", formulaTypeLabel(m_editorState.preview));
        } else if (formula) {
            ImGui::TextDisabled("(%s)", formulaTypeLabel(*formula));
        } else {
            ImGui::TextDisabled("(new)");
        }
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::TextWrapped("Enter an expression or equation. Supported forms include y=f(x), "
                           "z=f(x,y), F(x,y)=0, and F(x,y,z)=0.");
        ImGui::Text("Variables: x, y, z    %s", constantNamesText().c_str());
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        if (m_focusEditorInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusEditorInput = false;
        }

        const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 7.0f;
        inputTextMultilineString("##formula_editor", m_editorState.text,
                                 ImVec2(-1.0f, editorHeight));
        m_editorState.refreshPreview();
        const Model::Formula& editorPreview = m_editorState.preview;
        const size_t editorLength = m_editorState.text.size();

        ImGui::TextDisabled("Characters: %zu", editorLength);

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::BeginChild("FormulaEditorValidation", ImVec2(0.0f, 80.0f), true);
        ImGui::TextUnformatted("Live validation");
        if (!m_editorState.previewAvailable) {
            ImGui::TextDisabled("Start typing to validate the formula syntax and detected plot type.");
        } else if (editorPreview.isValid()) {
            ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f),
                               "Valid (%s)", formulaTypeLabel(editorPreview));
            ImGui::TextDisabled("Detected variables: %d   %s",
                                displayedVariableCount(editorPreview),
                                editorPreview.compiled.equation ? "Equation" : "Expression");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Invalid");
            const char* diagnostic = formulaDiagnosticText(editorPreview);
            if (diagnostic[0] != '\0') {
                ImGui::TextWrapped("%s", diagnostic);
            } else {
                ImGui::TextWrapped("Unable to parse the current formula.");
            }
        }
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        const bool canApply =
            m_editorState.previewAvailable && m_editorState.preview.isValid();
        {
            UiKit::DisabledScope disabled(!canApply);
            if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f))) {
                Model::Formula updated = addMode
                    ? m_editorState.buildAppliedFormula()
                    : *formula;
                if (!addMode) {
                    m_editorState.applyTo(updated);
                }

                if (updated.isValid()) {
                    if (addMode) {
                        actions.commands.emplace_back(AddFormulaCommand{ std::move(updated) });
                        ++m_nextColorIndex;
                    } else {
                        actions.commands.emplace_back(
                            UpdateFormulaCommand{ formula->id, std::move(updated) });
                    }
                    m_editorState.close();
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            m_editorState.close();
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
        }

        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen(kFormulaEditorPopupId)) {
        if (m_editorState.active()) {
            m_editorState.close();
        }
        m_focusEditorInput = false;
        m_selectedFunctionHelp = nullptr;
        m_selectedExampleHelpIndex = -1;
    }
}

FormulaPanelActions FormulaPanel::render(std::span<const Model::Formula> formulas) {
    FormulaPanelActions actions;

    ImGui::TextUnformatted("Formulas");
    ImGui::Separator();

    if (ImGui::Button("+ Add Formula", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        Model::Formula entry;
        int idx = m_nextColorIndex % kPaletteSize;
        applyPaletteColor(entry, idx);
        openAddEditor(std::move(entry));
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
                Model::Formula entry;
                entry.setExpression(preset.expression);
                int idx = m_nextColorIndex % kPaletteSize;
                applyPaletteColor(entry, idx);
                m_nextColorIndex++;
                entry.compile();
                actions.commands.emplace_back(AddFormulaCommand{ std::move(entry) });
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
    std::optional<Model::FormulaId> duplicateId;
    std::optional<Model::FormulaId> hideOthersId;
    std::optional<Model::FormulaId> moveId;
    std::optional<std::size_t> moveToIndex;

    if (formulas.empty()) {
        ImGui::TextDisabled("No formulas yet.");
    }

    for (int i = 0; i < static_cast<int>(formulas.size()); ++i) {
        const Model::Formula& formula = formulas[static_cast<std::size_t>(i)];
        const Components::FormulaCardResult card = Components::renderFormulaCard(
            formula,
            Components::FormulaCardContext{
                i,
                static_cast<int>(formulas.size()),
                ImGui::GetContentRegionAvail().x
            });

        if (card.visibilityChanged) {
            actions.commands.emplace_back(SetFormulaVisibilityCommand{
                formula.id,
                card.visible
            });
        }
        if (card.colorChanged) {
            actions.commands.emplace_back(SetFormulaColorCommand{
                formula.id,
                card.color
            });
        }
        if (card.zSliceChanged) {
            actions.commands.emplace_back(SetFormulaZSliceCommand{
                formula.id,
                card.zSlice
            });
        }

        switch (card.action.type) {
            case Components::FormulaCardActionType::Edit:
                openEditor(formula);
                break;
            case Components::FormulaCardActionType::Duplicate:
                duplicateId = card.action.formulaId;
                break;
            case Components::FormulaCardActionType::Copy: {
                const std::string expression = FormulaListActions::formulaExpression(formula);
                ImGui::SetClipboardText(expression.c_str());
                break;
            }
            case Components::FormulaCardActionType::HideOthers:
                hideOthersId = card.action.formulaId;
                break;
            case Components::FormulaCardActionType::MoveUp:
                moveId = card.action.formulaId;
                if (i > 0) {
                    moveToIndex = static_cast<std::size_t>(i - 1);
                }
                break;
            case Components::FormulaCardActionType::MoveDown:
                moveId = card.action.formulaId;
                if (i + 1 < static_cast<int>(formulas.size())) {
                    moveToIndex = static_cast<std::size_t>(i + 1);
                }
                break;
            case Components::FormulaCardActionType::RequestDelete:
                requestDeleteFormula(card.action.formulaId);
                break;
            case Components::FormulaCardActionType::None:
            default:
                break;
        }
    }

    if (hideOthersId.has_value()) {
        actions.commands.emplace_back(HideOtherFormulasCommand{ *hideOthersId });
    }
    if (duplicateId.has_value()) {
        actions.commands.emplace_back(DuplicateFormulaCommand{ *duplicateId });
    }
    if (moveId.has_value() && moveToIndex.has_value()) {
        actions.commands.emplace_back(MoveFormulaCommand{
            *moveId,
            *moveToIndex
        });
    }

    renderDeleteConfirmationDialog(formulas, actions);
    renderEditorDialog(formulas, actions);

    return actions;
}

} // namespace XpressFormula::UI
