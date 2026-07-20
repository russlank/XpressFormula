// Document.h - Revision-tracked project document aggregate.
#pragma once

#include "Formula.h"
#include "PlotSettings.h"
#include "../Core/ViewTransform.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace XpressFormula::Model {

class Document {
public:
    using Revision = std::uint64_t;

    class FormulaEdit {
    public:
        struct FormulaState;

        FormulaEdit(Document& document);
        FormulaEdit(const FormulaEdit&) = delete;
        FormulaEdit& operator=(const FormulaEdit&) = delete;
        FormulaEdit(FormulaEdit&& other) noexcept;
        FormulaEdit& operator=(FormulaEdit&& other) noexcept = delete;
        ~FormulaEdit();

        [[nodiscard]] std::vector<Formula>& get() noexcept;

    private:
        Document* m_document = nullptr;
        std::vector<FormulaState> m_before;
    };

    class ViewEdit {
    public:
        ViewEdit(Document& document);
        ViewEdit(const ViewEdit&) = delete;
        ViewEdit& operator=(const ViewEdit&) = delete;
        ViewEdit(ViewEdit&& other) noexcept;
        ViewEdit& operator=(ViewEdit&& other) noexcept = delete;
        ~ViewEdit();

        [[nodiscard]] Core::ViewTransform& get() noexcept;

    private:
        Document* m_document = nullptr;
        ViewState m_before;
    };

    class PlotSettingsEdit {
    public:
        PlotSettingsEdit(Document& document);
        PlotSettingsEdit(const PlotSettingsEdit&) = delete;
        PlotSettingsEdit& operator=(const PlotSettingsEdit&) = delete;
        PlotSettingsEdit(PlotSettingsEdit&& other) noexcept;
        PlotSettingsEdit& operator=(PlotSettingsEdit&& other) noexcept = delete;
        ~PlotSettingsEdit();

        [[nodiscard]] PlotSettings& get() noexcept;

    private:
        Document* m_document = nullptr;
        PlotSettings m_before;
    };

    [[nodiscard]] const std::vector<Formula>& formulas() const noexcept;
    [[nodiscard]] const Core::ViewTransform& viewTransform() const noexcept;
    [[nodiscard]] const ViewState& view() const noexcept;
    [[nodiscard]] const PlotSettings& plotSettings() const noexcept;

    [[nodiscard]] Revision revision() const noexcept;
    [[nodiscard]] Revision savedRevision() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;

    FormulaId addFormula(Formula formula);
    [[nodiscard]] bool updateFormula(FormulaId id, Formula formula);
    [[nodiscard]] bool duplicateFormula(FormulaId id);
    [[nodiscard]] bool removeFormula(FormulaId id);
    [[nodiscard]] bool moveFormula(FormulaId id, std::size_t toIndex);
    [[nodiscard]] bool setFormulaVisibility(FormulaId id, bool visible);
    [[nodiscard]] bool setFormulaColor(FormulaId id, const ColorRgba& color);
    [[nodiscard]] bool setFormulaZSlice(FormulaId id, double zSlice);
    [[nodiscard]] bool hideOtherFormulas(FormulaId id);
    [[nodiscard]] bool setViewState(const ViewState& state);
    [[nodiscard]] bool setViewTransform(const Core::ViewTransform& view);
    [[nodiscard]] bool setPlotSettings(const PlotSettings& settings);

    void replaceState(std::vector<Formula> formulas,
                      const Core::ViewTransform& view,
                      const PlotSettings& plotSettings,
                      bool markClean);
    void markSaved() noexcept;

    [[nodiscard]] FormulaEdit editFormulas();
    [[nodiscard]] ViewEdit editViewTransform();
    [[nodiscard]] PlotSettingsEdit editPlotSettings();
    void setViewport(const Viewport& viewport) noexcept;

private:
    [[nodiscard]] std::optional<std::size_t> findFormulaIndex(FormulaId id) const;
    void incrementRevision() noexcept;

    std::vector<Formula> m_formulas;
    Core::ViewTransform m_viewTransform;
    PlotSettings m_plotSettings;
    Revision m_revision = 0;
    Revision m_savedRevision = 0;
};

} // namespace XpressFormula::Model
