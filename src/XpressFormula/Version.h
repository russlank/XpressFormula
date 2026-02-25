// SPDX-License-Identifier: MIT
// Version.h - Centralized application version metadata.
#pragma once

// Semantic application version (major.minor.patch) plus an optional 4th Windows file-version part.
// We keep the numeric pieces separate so they can be reused by both resource metadata and UI text.
#define XF_VERSION_MAJOR 1
#define XF_VERSION_MINOR 3
#define XF_VERSION_PATCH 3
#define XF_VERSION_BUILD 0

// Windows VERSIONINFO resources expect a comma-separated numeric tuple, not a quoted string.
// Example expansion: 1, 3, 3, 0
#define XF_VERSION_FILEVERSION XF_VERSION_MAJOR, XF_VERSION_MINOR, XF_VERSION_PATCH, XF_VERSION_BUILD

// String helpers are used to derive the display version text from the numeric macros above so the
// semantic version only needs to be edited in one place.
#define XF_STRINGIZE_IMPL(x) #x
#define XF_STRINGIZE(x) XF_STRINGIZE_IMPL(x)

// Human-readable semantic version used in the UI, logs, and default build metadata.
// Example expansion: "1.3.3"
#define XF_VERSION_STRING \
    XF_STRINGIZE(XF_VERSION_MAJOR) "." XF_STRINGIZE(XF_VERSION_MINOR) "." XF_STRINGIZE(XF_VERSION_PATCH)

#define XF_WIDE_IMPL(x) L##x
#define XF_WIDE(x) XF_WIDE_IMPL(x)
// Wide-string form for Win32 Unicode APIs (window title, etc.).
#define XF_VERSION_WSTRING XF_WIDE(XF_VERSION_STRING)

#ifndef XF_BUILD_REPO_URL
#define XF_BUILD_REPO_URL "unknown"
#endif

#ifndef XF_BUILD_BRANCH
#define XF_BUILD_BRANCH "unknown"
#endif

#ifndef XF_BUILD_COMMIT
#define XF_BUILD_COMMIT "unknown"
#endif

#ifndef XF_BUILD_VERSION
// Build pipelines may override this (e.g. prerelease suffixes or CI-injected version strings).
// Fallback to the app semantic version if no override is provided.
#define XF_BUILD_VERSION XF_VERSION_STRING
#endif
