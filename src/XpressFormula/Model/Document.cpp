// Document.cpp - Revision-tracked project document aggregate.
#include "Document.h"
#include "PlotPolicy.h"
#include "../Core/InputLimits.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <utility>

namespace XpressFormula::Model {
namespace {

bool sameViewState(const ViewState& lhs, const ViewState& rhs) noexcept {
    return lhs.centerX == rhs.centerX &&
           lhs.centerY == rhs.centerY &&
           lhs.scaleX == rhs.scaleX &&
           lhs.scaleY == rhs.scaleY;
}

bool sameColor(const ColorRgba& lhs, const ColorRgba& rhs) noexcept {
    return lhs.channels == rhs.channels;
}

bool samePlotSettings(const PlotSettings& lhs, const PlotSettings& rhs) noexcept {
    return lhs.xyRenderModePreference == rhs.xyRenderModePreference &&
           lhs.hudMode == rhs.hudMode &&
           lhs.optimizeRendering == rhs.optimizeRendering &&
           lhs.showGrid == rhs.showGrid &&
           lhs.showCoordinates == rhs.showCoordinates &&
           lhs.showWires == rhs.showWires &&
           lhs.azimuthDeg == rhs.azimuthDeg &&
           lhs.elevationDeg == rhs.elevationDeg &&
           lhs.zScale == rhs.zScale &&
           lhs.surfaceResolution == rhs.surfaceResolution &&
           lhs.implicitSurfaceResolution == rhs.implicitSurfaceResolution &&
           lhs.surfaceOpacity == rhs.surfaceOpacity &&
           lhs.wireOpacity == rhs.wireOpacity &&
           lhs.wireThickness == rhs.wireThickness &&
           lhs.wireStride == rhs.wireStride &&
           lhs.showSurfaceEnvelope == rhs.showSurfaceEnvelope &&
           lhs.envelopeThickness == rhs.envelopeThickness &&
           lhs.showAxisTriad == rhs.showAxisTriad &&
           lhs.autoRotate == rhs.autoRotate &&
           lhs.autoRotateSpeedDegPerSec == rhs.autoRotateSpeedDegPerSec &&
           lhs.heatmapOpacity == rhs.heatmapOpacity;
}

float normalizeColorChannel(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
}

ColorRgba normalizeColor(ColorRgba color) noexcept {
    for (float& channel : color.channels) {
        channel = normalizeColorChannel(channel);
    }
    return color;
}

double normalizeZSlice(double zSlice) noexcept {
    if (!std::isfinite(zSlice)) {
        return 0.0;
    }
    constexpr double limit = static_cast<double>((std::numeric_limits<float>::max)());
    return std::clamp(zSlice, -limit, limit);
}

ViewState normalizeViewState(ViewState state) noexcept {
    if (!std::isfinite(state.centerX)) {
        state.centerX = 0.0;
    }
    if (!std::isfinite(state.centerY)) {
        state.centerY = 0.0;
    }
    state.scaleX = std::clamp(
        std::isfinite(state.scaleX) ? state.scaleX : 60.0,
        Core::InputLimits::kMinViewScale,
        Core::InputLimits::kMaxViewScale);
    state.scaleY = std::clamp(
        std::isfinite(state.scaleY) ? state.scaleY : 60.0,
        Core::InputLimits::kMinViewScale,
        Core::InputLimits::kMaxViewScale);
    return state;
}

bool containsFormulaId(const std::vector<Formula>& formulas,
                       FormulaId id,
                       std::size_t count) noexcept {
    for (std::size_t i = 0; i < count && i < formulas.size(); ++i) {
        if (formulas[i].id == id) {
            return true;
        }
    }
    return false;
}

void normalizeFormula(Formula& formula,
                      const std::vector<Formula>& existing,
                      std::size_t existingCount) {
    while (formula.id == 0 || containsFormulaId(existing, formula.id, existingCount)) {
        formula.assignNewId();
    }
    formula.color = normalizeColor(formula.color);
    formula.zSlice = normalizeZSlice(formula.zSlice);
}

PlotSettings normalizedPlotSettings(PlotSettings settings) {
    normalizePlotSettings(settings);
    return settings;
}

} // namespace

struct Document::FormulaEdit::FormulaState {
    FormulaId id = 0;
    std::string expression;
    ColorRgba color;
    bool visible = true;
    double zSlice = 0.0;
};

namespace {

Document::FormulaEdit::FormulaState captureFormulaState(const Formula& formula) {
    Document::FormulaEdit::FormulaState state;
    state.id = formula.id;
    state.expression = formula.expression;
    state.color = formula.color;
    state.visible = formula.visible;
    state.zSlice = formula.zSlice;
    return state;
}

std::vector<Document::FormulaEdit::FormulaState> captureFormulaStates(
    const std::vector<Formula>& formulas) {
    std::vector<Document::FormulaEdit::FormulaState> states;
    states.reserve(formulas.size());
    for (const Formula& formula : formulas) {
        states.push_back(captureFormulaState(formula));
    }
    return states;
}

bool sameFormulaState(const Document::FormulaEdit::FormulaState& lhs,
                      const Formula& rhs) {
    return lhs.id == rhs.id &&
           lhs.expression == rhs.expression &&
           sameColor(lhs.color, rhs.color) &&
           lhs.visible == rhs.visible &&
           lhs.zSlice == rhs.zSlice;
}

bool sameFormulaStates(const std::vector<Document::FormulaEdit::FormulaState>& lhs,
                       const std::vector<Formula>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!sameFormulaState(lhs[i], rhs[i])) {
            return false;
        }
    }
    return true;
}

bool sameFormulaPersistentState(const Formula& lhs, const Formula& rhs) {
    return lhs.id == rhs.id &&
           lhs.expression == rhs.expression &&
           sameColor(lhs.color, rhs.color) &&
           lhs.visible == rhs.visible &&
           lhs.zSlice == rhs.zSlice;
}

} // namespace

Document::FormulaEdit::FormulaEdit(Document& document)
    : m_document(&document),
      m_before(captureFormulaStates(document.m_formulas)),
      m_restoreBefore(document.m_formulas) {
}

Document::FormulaEdit::FormulaEdit(FormulaEdit&& other) noexcept
    : m_document(other.m_document),
      m_before(std::move(other.m_before)),
      m_restoreBefore(std::move(other.m_restoreBefore)) {
    other.m_document = nullptr;
}

Document::FormulaEdit::~FormulaEdit() {
    if (m_document) {
        if (m_document->m_formulas.size() > Core::InputLimits::kMaxProjectFormulas) {
            m_document->m_formulas = std::move(m_restoreBefore);
            return;
        }
        for (std::size_t i = 0; i < m_document->m_formulas.size(); ++i) {
            normalizeFormula(m_document->m_formulas[i], m_document->m_formulas, i);
        }
        if (!sameFormulaStates(m_before, m_document->m_formulas)) {
            m_document->incrementRevision();
        }
    }
}

std::vector<Formula>& Document::FormulaEdit::get() noexcept {
    return m_document->m_formulas;
}

Document::ViewEdit::ViewEdit(Document& document)
    : m_document(&document),
      m_before(document.m_viewTransform.state) {
}

Document::ViewEdit::ViewEdit(ViewEdit&& other) noexcept
    : m_document(other.m_document),
      m_before(other.m_before) {
    other.m_document = nullptr;
}

Document::ViewEdit::~ViewEdit() {
    if (m_document) {
        m_document->m_viewTransform.state = normalizeViewState(m_document->m_viewTransform.state);
        if (!sameViewState(m_before, m_document->m_viewTransform.state)) {
            m_document->incrementRevision();
        }
    }
}

Core::ViewTransform& Document::ViewEdit::get() noexcept {
    return m_document->m_viewTransform;
}

Document::PlotSettingsEdit::PlotSettingsEdit(Document& document)
    : m_document(&document),
      m_before(document.m_plotSettings) {
}

Document::PlotSettingsEdit::PlotSettingsEdit(PlotSettingsEdit&& other) noexcept
    : m_document(other.m_document),
      m_before(other.m_before) {
    other.m_document = nullptr;
}

Document::PlotSettingsEdit::~PlotSettingsEdit() {
    if (m_document) {
        m_document->m_plotSettings = normalizedPlotSettings(m_document->m_plotSettings);
        if (!samePlotSettings(m_before, m_document->m_plotSettings)) {
            m_document->incrementRevision();
        }
    }
}

PlotSettings& Document::PlotSettingsEdit::get() noexcept {
    return m_document->m_plotSettings;
}

const std::vector<Formula>& Document::formulas() const noexcept {
    return m_formulas;
}

const Core::ViewTransform& Document::viewTransform() const noexcept {
    return m_viewTransform;
}

const ViewState& Document::view() const noexcept {
    return m_viewTransform.state;
}

const PlotSettings& Document::plotSettings() const noexcept {
    return m_plotSettings;
}

Document::Revision Document::revision() const noexcept {
    return m_revision;
}

Document::Revision Document::savedRevision() const noexcept {
    return m_savedRevision;
}

bool Document::dirty() const noexcept {
    return m_revision != m_savedRevision;
}

FormulaId Document::addFormula(Formula formula) {
    if (m_formulas.size() >= Core::InputLimits::kMaxProjectFormulas) {
        return 0;
    }

    normalizeFormula(formula, m_formulas, m_formulas.size());
    const FormulaId id = formula.id;
    m_formulas.push_back(std::move(formula));
    incrementRevision();
    return id;
}

bool Document::updateFormula(FormulaId id, Formula formula) {
    const std::optional<std::size_t> index = findFormulaIndex(id);
    if (!index.has_value()) {
        return false;
    }

    formula.id = id;
    formula.color = normalizeColor(formula.color);
    formula.zSlice = normalizeZSlice(formula.zSlice);
    if (sameFormulaPersistentState(m_formulas[*index], formula)) {
        return false;
    }

    m_formulas[*index] = std::move(formula);
    incrementRevision();
    return true;
}

bool Document::duplicateFormula(FormulaId id) {
    if (m_formulas.size() >= Core::InputLimits::kMaxProjectFormulas) {
        return false;
    }

    const std::optional<std::size_t> index = findFormulaIndex(id);
    if (!index.has_value()) {
        return false;
    }

    Formula duplicate = m_formulas[*index];
    duplicate.assignNewId();
    duplicate.color = normalizeColor(duplicate.color);
    duplicate.zSlice = normalizeZSlice(duplicate.zSlice);
    duplicate.hasCompiledExpression = false;
    duplicate.lastCompiledExpression.clear();
    duplicate.compile(true);
    m_formulas.insert(m_formulas.begin() + static_cast<std::ptrdiff_t>(*index + 1),
                      std::move(duplicate));
    incrementRevision();
    return true;
}

bool Document::removeFormula(FormulaId id) {
    const std::optional<std::size_t> index = findFormulaIndex(id);
    if (!index.has_value()) {
        return false;
    }

    m_formulas.erase(m_formulas.begin() + static_cast<std::ptrdiff_t>(*index));
    incrementRevision();
    return true;
}

bool Document::moveFormula(FormulaId id, std::size_t toIndex) {
    const std::optional<std::size_t> fromIndex = findFormulaIndex(id);
    if (!fromIndex.has_value() || toIndex >= m_formulas.size()) {
        return false;
    }
    if (*fromIndex == toIndex) {
        return false;
    }

    Formula moved = std::move(m_formulas[*fromIndex]);
    m_formulas.erase(m_formulas.begin() + static_cast<std::ptrdiff_t>(*fromIndex));
    m_formulas.insert(m_formulas.begin() + static_cast<std::ptrdiff_t>(toIndex),
                      std::move(moved));
    incrementRevision();
    return true;
}

bool Document::setFormulaVisibility(FormulaId id, bool visible) {
    const std::optional<std::size_t> index = findFormulaIndex(id);
    if (!index.has_value() || m_formulas[*index].visible == visible) {
        return false;
    }

    m_formulas[*index].visible = visible;
    incrementRevision();
    return true;
}

bool Document::setFormulaColor(FormulaId id, const ColorRgba& color) {
    const std::optional<std::size_t> index = findFormulaIndex(id);
    const ColorRgba normalized = normalizeColor(color);
    if (!index.has_value() || sameColor(m_formulas[*index].color, normalized)) {
        return false;
    }

    m_formulas[*index].color = normalized;
    incrementRevision();
    return true;
}

bool Document::setFormulaZSlice(FormulaId id, double zSlice) {
    const std::optional<std::size_t> index = findFormulaIndex(id);
    const double normalized = normalizeZSlice(zSlice);
    if (!index.has_value() || m_formulas[*index].zSlice == normalized) {
        return false;
    }

    m_formulas[*index].zSlice = normalized;
    incrementRevision();
    return true;
}

bool Document::hideOtherFormulas(FormulaId id) {
    if (!findFormulaIndex(id).has_value()) {
        return false;
    }

    bool changed = false;
    for (Formula& formula : m_formulas) {
        const bool visible = formula.id == id;
        if (formula.visible != visible) {
            formula.visible = visible;
            changed = true;
        }
    }

    if (changed) {
        incrementRevision();
    }
    return changed;
}

bool Document::setViewState(const ViewState& state) {
    const ViewState normalized = normalizeViewState(state);
    if (sameViewState(m_viewTransform.state, normalized)) {
        return false;
    }

    m_viewTransform.state = normalized;
    incrementRevision();
    return true;
}

bool Document::setViewTransform(const Core::ViewTransform& view) {
    Core::ViewTransform normalized = view;
    normalized.state = normalizeViewState(normalized.state);
    const bool persistentChanged = !sameViewState(m_viewTransform.state, normalized.state);
    m_viewTransform = normalized;
    if (persistentChanged) {
        incrementRevision();
    }
    return persistentChanged;
}

bool Document::setPlotSettings(const PlotSettings& settings) {
    const PlotSettings normalized = normalizedPlotSettings(settings);
    if (samePlotSettings(m_plotSettings, normalized)) {
        return false;
    }

    m_plotSettings = normalized;
    incrementRevision();
    return true;
}

bool Document::replaceState(std::vector<Formula> formulas,
                            const Core::ViewTransform& view,
                            const PlotSettings& plotSettings,
                            bool markClean) {
    if (formulas.size() > Core::InputLimits::kMaxProjectFormulas) {
        return false;
    }

    for (std::size_t i = 0; i < formulas.size(); ++i) {
        normalizeFormula(formulas[i], formulas, i);
    }
    Core::ViewTransform normalizedView = view;
    normalizedView.state = normalizeViewState(normalizedView.state);
    PlotSettings normalizedPlot = normalizedPlotSettings(plotSettings);

    m_formulas = std::move(formulas);
    m_viewTransform = normalizedView;
    m_plotSettings = normalizedPlot;
    incrementRevision();
    if (markClean) {
        markSaved();
    }
    return true;
}

void Document::markSaved() noexcept {
    m_savedRevision = m_revision;
}

Document::FormulaEdit Document::editFormulas() {
    return FormulaEdit(*this);
}

Document::ViewEdit Document::editViewTransform() {
    return ViewEdit(*this);
}

Document::PlotSettingsEdit Document::editPlotSettings() {
    return PlotSettingsEdit(*this);
}

void Document::setViewport(const Viewport& viewport) noexcept {
    m_viewTransform.viewport = viewport;
}

std::optional<std::size_t> Document::findFormulaIndex(FormulaId id) const {
    for (std::size_t index = 0; index < m_formulas.size(); ++index) {
        if (m_formulas[index].id == id) {
            return index;
        }
    }
    return std::nullopt;
}

void Document::incrementRevision() noexcept {
    ++m_revision;
}

} // namespace XpressFormula::Model
