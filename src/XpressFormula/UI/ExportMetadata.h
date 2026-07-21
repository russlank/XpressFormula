// SPDX-License-Identifier: MIT
// ExportMetadata.h - Compatibility shim for infrastructure-owned export metadata helpers.
#pragma once

#include "../Infrastructure/Export/ExportMetadataSerializer.h"

namespace XpressFormula::UI {

using Infrastructure::Export::ExportAppMetadata;
using Infrastructure::Export::ExportCameraMetadata;
using Infrastructure::Export::ExportDisplayMetadata;
using Infrastructure::Export::ExportFormulaMetadata;
using Infrastructure::Export::ExportImageMetadata;
using Infrastructure::Export::ExportMetadataModel;
using Infrastructure::Export::ExportViewMetadata;
using Infrastructure::Export::exportActualFormatLabel;
using Infrastructure::Export::exportMetadataSidecarPath;
using Infrastructure::Export::exportMetadataTempPath;
using Infrastructure::Export::jsonBool;
using Infrastructure::Export::jsonEscape;
using Infrastructure::Export::kExportMetadataSchemaVersion;
using Infrastructure::Export::makeExportCameraMetadata;
using Infrastructure::Export::makeExportDisplayMetadata;
using Infrastructure::Export::makeExportFormulaMetadata;
using Infrastructure::Export::makeExportMetadataModel;
using Infrastructure::Export::makeExportViewMetadata;
using Infrastructure::Export::serializeExportMetadata;

} // namespace XpressFormula::UI
