// SPDX-License-Identifier: MIT
// Version.h - Centralized application version metadata.
#pragma once

#define XF_VERSION_MAJOR 1
#define XF_VERSION_MINOR 1
#define XF_VERSION_PATCH 0
#define XF_VERSION_BUILD 0

#define XF_VERSION_FILEVERSION XF_VERSION_MAJOR, XF_VERSION_MINOR, XF_VERSION_PATCH, XF_VERSION_BUILD
#define XF_VERSION_STRING "1.1.0"

#define XF_WIDE_IMPL(x) L##x
#define XF_WIDE(x) XF_WIDE_IMPL(x)
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
#define XF_BUILD_VERSION XF_VERSION_STRING
#endif
