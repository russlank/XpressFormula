// FormulaCard.cpp - Responsive formula-list card implementation.
#include "FormulaCard.h"
#include "../FormulaListActions.h"
#include "../UiKit/ResponsiveLayout.h"
#include "../UiKit/UiMetrics.h"
#include "../UiKit/UiScopes.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace XpressFormula::UI::Components {

namespace {

std::string ellipsizeForWidth(const char* text, float maxWidth) {
    std::string value = text ? text : "";
    if (value.empty()) {
        value = "(empty formula)";
    }

    if (maxWidth <= 0.0f || ImGui::CalcTextSize(value.c_str()).x <= maxWidth) {
        return value;
    }

    constexpr const char* ellipsis = "...";
    const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
    if (maxWidth <= ellipsisWidth) {
        return ellipsis;
    }

    while (!value.empty() &&
           ImGui::CalcTextSize(value.c_str()).x + ellipsisWidth > maxWidth) {
        value.pop_back();
    }
    return value.empty() ? ellipsis : value + ellipsis;
}

const char* validationLabel(const FormulaEntry& formula) {
    if (formula.isValid()) {
        return "Valid";
    }
    return FormulaListActions::formulaExpression(formula).empty() ? "Empty" : "Invalid";
}

ImVec4 validationColor(const FormulaEntry& formula) {
    if (formula.isValid()) {
        return ImVec4(0.44f, 0.92f, 0.52f, 1.0f);
    }
    if (FormulaListActions::formulaExpression(formula).empty()) {
        return ImVec4(0.72f, 0.72f, 0.78f, 1.0f);
    }
    return ImVec4(1.0f, 0.45f, 0.38f, 1.0f);
}

void showWrappedTooltip(const char* first, const char* second = nullptr) {
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
    }
    ImGui::EndTooltip();
}

FormulaCardAction makeAction(FormulaCardActionType type, int index) {
    return FormulaCardAction{ type, index };
}

bool hasAction(const FormulaCardAction& action) {
    return action.type != FormulaCardActionType::None;
}

void drawMetadata(const FormulaEntry& formula,
                  bool includeValidation,
                  const char* validationText) {
    if (includeValidation) {
        UiKit::StyleColorScope color(ImGuiCol_Text, validationColor(formula));
        ImGui::TextUnformatted(validationText);

        if (!formula.isValid()) {
            ImGui::SameLine();
            if (!formula.error.empty()) {
                ImGui::TextDisabled("| hover for error details");
                if (ImGui::IsItemHovered()) {
                    showWrappedTooltip("Invalid formula", formula.error.c_str());
                }
            } else {
                ImGui::TextDisabled("| no parsed formula yet");
            }
            return;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
    }

    if (formula.isValid()) {
        ImGui::TextDisabled("%s | %s | variables: %d",
                            formula.typeLabel(),
                            formula.isEquation ? "Equation" : "Expression",
                            formula.variableCount);
    } else if (!formula.error.empty()) {
        UiKit::StyleColorScope color(ImGuiCol_Text, validationColor(formula));
        ImGui::TextUnformatted("Invalid");
        ImGui::SameLine();
        ImGui::TextDisabled("| hover for error details");
        if (ImGui::IsItemHovered()) {
            showWrappedTooltip("Invalid formula", formula.error.c_str());
        }
    } else {
        ImGui::TextDisabled("Empty | no parsed formula yet");
    }
}

} // namespace

FormulaCardAction renderFormulaCard(FormulaEntry& formula,
                                    const FormulaCardContext& context) {
    FormulaCardAction action;
    UiKit::IdScope cardId(context.index);

    const bool showZSlice = formula.renderKind == FormulaRenderKind::ScalarField3D &&
        formula.isValid();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float frameHeight = ImGui::GetFrameHeightWithSpacing();
    const float editWidth = ImGui::CalcTextSize("Edit").x + style.FramePadding.x * 2.0f;
    const float actionsWidth = ImGui::CalcTextSize("...").x + style.FramePadding.x * 2.0f;
    const float deleteWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x * 2.0f;
    const std::string title = "Formula " + std::to_string(context.index + 1);
    const char* validationText = validationLabel(formula);

    const UiKit::FormulaCardLayoutPlan plan = UiKit::planFormulaCard({
        context.availableWidth,
        showZSlice,
        ImGui::CalcTextSize(title.c_str()).x,
        ImGui::CalcTextSize(validationText).x,
        editWidth,
        actionsWidth,
        deleteWidth
    });

    const int nonZRows = plan.rowCount - (showZSlice ? 1 : 0);
    const int textRows = std::max(0, nonZRows - 1);
    const float cardHeight = std::ceil(
        style.WindowPadding.y * 2.0f +
        frameHeight +
        lineHeight * static_cast<float>(textRows) +
        (showZSlice ? frameHeight : 0.0f));

    ImGui::BeginChild("##formula_card", ImVec2(0.0f, cardHeight), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    auto setActionIfEmpty = [&](FormulaCardActionType type) {
        if (!hasAction(action)) {
            action = makeAction(type, context.index);
        }
    };

    auto drawFormulaActionsMenu = [&]() {
        if (ImGui::MenuItem("Edit")) {
            setActionIfEmpty(FormulaCardActionType::Edit);
        }
        if (ImGui::MenuItem("Duplicate")) {
            setActionIfEmpty(FormulaCardActionType::Duplicate);
        }
        if (ImGui::MenuItem("Copy Formula")) {
            setActionIfEmpty(FormulaCardActionType::Copy);
        }
        if (ImGui::MenuItem("Hide Others")) {
            setActionIfEmpty(FormulaCardActionType::HideOthers);
        }
        ImGui::Separator();
        {
            UiKit::DisabledScope disabled(context.index <= 0);
            if (ImGui::MenuItem("Move Up")) {
                setActionIfEmpty(FormulaCardActionType::MoveUp);
            }
        }
        {
            UiKit::DisabledScope disabled(context.index + 1 >= context.count);
            if (ImGui::MenuItem("Move Down")) {
                setActionIfEmpty(FormulaCardActionType::MoveDown);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            setActionIfEmpty(FormulaCardActionType::RequestDelete);
        }
    };

    ImGui::Checkbox("##visible", &formula.visible);
    ImGui::SetItemTooltip("%s Formula %d.", formula.visible ? "Hide" : "Show", context.index + 1);
    ImGui::SameLine();

    ImGui::ColorEdit4("##color", formula.color,
                      ImGuiColorEditFlags_NoInputs |
                      ImGuiColorEditFlags_NoLabel |
                      ImGuiColorEditFlags_NoTooltip);
    ImGui::SetItemTooltip("Set Formula %d color.", context.index + 1);
    ImGui::SameLine();

    ImGui::TextUnformatted(title.c_str());
    if (plan.showStatusInHeader) {
        ImGui::SameLine();
        UiKit::StyleColorScope color(ImGuiCol_Text, validationColor(formula));
        ImGui::TextUnformatted(validationText);
        if (ImGui::IsItemHovered() && !formula.error.empty()) {
            showWrappedTooltip("Invalid formula", formula.error.c_str());
        }
    }

    float totalButtonWidth = actionsWidth;
    if (plan.showEditButton) {
        totalButtonWidth += editWidth + style.ItemSpacing.x;
    }
    if (plan.showDeleteButton) {
        totalButtonWidth += deleteWidth + style.ItemSpacing.x;
    }
    const float rightAlignedX = ImGui::GetWindowContentRegionMax().x - totalButtonWidth;
    if (ImGui::GetCursorPosX() < rightAlignedX) {
        ImGui::SameLine(rightAlignedX);
    } else {
        ImGui::SameLine();
    }

    if (plan.showEditButton) {
        if (ImGui::SmallButton("Edit")) {
            setActionIfEmpty(FormulaCardActionType::Edit);
        }
        ImGui::SetItemTooltip("Edit Formula %d.", context.index + 1);
        ImGui::SameLine();
    }

    if (ImGui::SmallButton("...##actions")) {
        ImGui::OpenPopup("FormulaActionsMenu");
    }
    ImGui::SetItemTooltip("Open formula actions.");
    if (ImGui::BeginPopup("FormulaActionsMenu")) {
        drawFormulaActionsMenu();
        ImGui::EndPopup();
    }

    if (plan.showDeleteButton) {
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            setActionIfEmpty(FormulaCardActionType::RequestDelete);
        }
        ImGui::SetItemTooltip("Delete Formula %d.", context.index + 1);
    }

    if (!plan.showStatusInHeader) {
        drawMetadata(formula, true, validationText);
    }

    const std::string expression = FormulaListActions::formulaExpression(formula);
    const float expressionWidth = ImGui::GetContentRegionAvail().x;
    const float expressionRowHeight = ImGui::GetTextLineHeightWithSpacing();
    const std::string expressionPreview =
        ellipsizeForWidth(expression.c_str(), expressionWidth - 4.0f);
    ImGui::Selectable("##expression", false,
                      ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(expressionWidth, expressionRowHeight));
    const bool expressionHovered = ImGui::IsItemHovered();
    if (expressionHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        setActionIfEmpty(FormulaCardActionType::Edit);
    }
    const ImVec2 expressionMin = ImGui::GetItemRectMin();
    const float expressionTextY =
        expressionMin.y + (expressionRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    const ImU32 expressionColor = ImGui::GetColorU32(
        expression.empty() ? ImGuiCol_TextDisabled : ImGuiCol_Text);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(expressionMin.x, expressionTextY),
        expressionColor,
        expressionPreview.c_str());

    if (expressionHovered) {
        const char* expressionText = expression.empty() ? "(empty formula)" : expression.c_str();
        showWrappedTooltip(expressionText, "Double-click to edit. Right-click for actions.");
    }
    if (ImGui::BeginPopupContextItem("FormulaExpressionContext")) {
        drawFormulaActionsMenu();
        ImGui::EndPopup();
    }

    if (plan.showStatusInHeader) {
        drawMetadata(formula, false, validationText);
    }

    if (showZSlice) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (formula.isEquation) {
            ImGui::SliderFloat("z slice / center", &formula.zSlice, -10.0f, 10.0f, "z = %.2f");
        } else {
            ImGui::SliderFloat("z slice", &formula.zSlice, -10.0f, 10.0f, "z = %.2f");
        }
    }

    ImGui::EndChild();
    return action;
}

} // namespace XpressFormula::UI::Components
