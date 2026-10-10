#pragma once

// ─── Firmware version alias ──────────────────────────────────────────────────
//
// The version itself lives in include/config.h as FIRMWARE_VERSION (the
// cyd-web-installer CI reads it there). FW_VERSION_STR is kept as an alias so
// existing code is unchanged. Bump FIRMWARE_VERSION and everything in the C++
// codebase that displays or logs the version picks it up automatically:
//
//   • WebUI HTML <title>   ─ src/adapters/WebUIServer.cpp
//   • WebUI brand badge    ─ src/adapters/WebUIServer.cpp
//   • Serial boot banner   ─ src/main.cpp
//
// HTML inside the WebUIServer PROGMEM blob uses adjacent string-literal
// concatenation:
//
//   R"rawlit(<title>… CYD v)rawlit" FW_VERSION_STR R"rawlit(</title>…)rawlit"
//
// The preprocessor expands FW_VERSION_STR, the compiler concatenates the
// three neighbouring string literals at parse time, and the result is one
// PROGMEM array with zero runtime cost.
//
// ─── Things this macro CANNOT auto-update ────────────────────────────────────
// Markdown can't include C macros, so when you bump the version you also need
// to touch by hand:
//
//   • README.md           "Current release: vX.Y.Z" line
//   • CLAUDE.md           "Current dev version" line
//   • CHANGELOG.md        [Unreleased] / [X.Y.Z] heading + entries
//
// Suffix conventions: see include/config.h.
// ─────────────────────────────────────────────────────────────────────────────

#include "config.h"

#define FW_VERSION_STR FIRMWARE_VERSION
