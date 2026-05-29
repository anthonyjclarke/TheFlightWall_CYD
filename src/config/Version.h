#pragma once

// ─── Single source of truth for the firmware release version ─────────────────
//
// Bump FW_VERSION_STR HERE and everything in the C++ codebase that displays
// or logs the version picks it up automatically:
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
// Suffix conventions (matches the rest of the project):
//   • "1.3.0-dev"   work-in-progress on the dev branch
//   • "1.3.0-rc1"   release candidate
//   • "1.3.0"       tagged release on main
// ─────────────────────────────────────────────────────────────────────────────

#define FW_VERSION_STR "1.3.0"
