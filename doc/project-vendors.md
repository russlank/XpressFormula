<!-- SPDX-License-Identifier: MIT -->
# Project Vendors

This document tracks third-party code vendored directly into this repository.

## Dependency Table

| Component | Local Path | Upstream | License | Pinned Revision | Purpose |
|---|---|---|---|---|---|
| Dear ImGui | `src/vendor/imgui` | https://github.com/ocornut/imgui | MIT | https://github.com/ocornut/imgui/commit/37b7a7a9dfc4a775c371bbd0ab8ceaf132c78fce | Immediate-mode GUI + backends (`Win32`, `DX11`) |

## Usage Notes

- XpressFormula compiles ImGui source directly from the vendored tree.
- Project files include:
  - core ImGui source files (`imgui*.cpp`)
  - backend files (`imgui_impl_win32.cpp`, `imgui_impl_dx11.cpp`)

## Update Process (Suggested)

1. Choose and record an upstream commit/tag.
2. Replace/update files under `src/vendor/imgui`.
3. Build and run:
   - application (`Debug|x64`)
   - tests (`XpressFormula.Tests`)
4. Update this document with the new pinned revision.

## License

This documentation file is licensed under the MIT License. See `../LICENSE`.
Vendored dependencies may use their own licenses as listed above.
