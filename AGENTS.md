# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project overview

Carbon Mesh is a C++ library for serializing 3D mesh, skeleton and animation data in the **CMF** (Carbon Mesh Format) binary format, plus a small animation runtime. The repo also contains two optional tools:

| Component | Path | Target | Notes |
|---|---|---|---|
| Library | `include/cmf/`, `src/cmf/` | `CarbonMesh` (shared lib) | Public API, always built |
| Unit tests | `src/cmf/test/` | `cmftest` | GoogleTest, always built |
| CLI processor | `src/processor/` | `cmfprocessor` | Requires `-DBUILD_TOOLS=ON` |
| Viewer | `src/viewer/` | `CarbonMeshViewer` | Vulkan/GLFW/ImGui app, requires `-DBUILD_TOOLS=ON` |
| Docs | `doc/source/` | `Sphinx`, `Doxygen` | reStructuredText, requires `-DBUILD_DOCUMENTATION=ON` |

Third-party code lives in `vendor/` as git submodules (vcpkg, the carbonengine vcpkg registry, ufbx, MoltenVK, Font Awesome). **Do not modify anything under `vendor/`.**

## Build

Dependencies come from vcpkg (manifest mode, `vcpkg.json`) using the vendored vcpkg submodule. Initialize submodules first:

```sh
git submodule update --init --recursive
```

Configure with a preset from `CMakePresets.json`. Presets are `<arch>-<os>-<flavor>`, where the flavor is `debug`, `internal`, `release` or `trinitydev`. Each preset builds into `.cmake-build-<presetName>/`.
If configuring for multi-config projects, only build configurations that match the preset.

```sh
# Library + tests only
cmake --preset x64-windows-release
cmake --build .cmake-build-x64-windows-release --config Release

# Library + tools (cmfprocessor, viewer)
cmake --preset x64-windows-release -DBUILD_TOOLS=ON
cmake --build .cmake-build-x64-windows-release --config Release
# or a single target: --target CarbonMesh | cmftest | cmfprocessor | CarbonMeshViewer
```

The viewer build always prints a glslc warning, `orientationgizmo.vert:4: warning: '#extension' : extension not supported: GL_KHR_vulkan_glsl`. It is known, doesn't fail the build, and doesn't need fixing.

macOS presets: `arm64-osx-*` and `x64-osx-*`.

CMake options:

- `BUILD_TOOLS` (default OFF): builds `cmfprocessor` and the viewer, and enables the vcpkg `tools` feature.
- `WITH_SIMPLYGON` (default OFF): uses Simplygon for LOD generation. Needs `BUILD_TOOLS=ON` and a Simplygon license/SDK.
- `BUILD_DOCUMENTATION` (default OFF locally, ON under TeamCity): builds the Sphinx and Doxygen docs.
- `INSTALL_TO_MONOLITH` (default OFF): installs into a Perforce-style vendor layout instead of the vcpkg layout.

`CMakeUserPresets.json` is gitignored and holds each developer's local settings. Don't commit it or depend on what's in it.

A first configure is slow because vcpkg builds every dependency. Reuse an existing `.cmake-build-*` directory when there is one.


## Tests

```sh
ctest --test-dir .cmake-build-x64-windows-release -C Release
# or run the binary directly for gtest filters:
.cmake-build-x64-windows-release/src/cmf/test/Release/cmftest --gtest_filter=Writer*
```

When you add a test file, add it to `CMF_TEST_SOURCES` in `src/cmf/test/CMakeLists.txt`. Library changes should include tests.

## Code style

Formatting comes from `.clang-format`, and CI (`.github/workflows/cpp_format.yml`) **fails PRs that aren't clang-format clean**. Check it with `python scripts/run_clang_tidy.py`, which runs the clang-format check on your changed lines before clang-tidy (see [Static analysis](#static-analysis-clang-tidy)). Use `--fix-format` to reformat your changed lines and `--format-only` for a quick check without a build. Don't run `clang-format -i` on whole files, because it can reformat code you didn't change. Locally the script uses the clang-format that ships with Visual Studio (22.1.3 at the time of writing). CI uses the cpp-linter action's default version. If CI reports something the local check didn't, CI is authoritative. Key points:

- **Tabs** for indentation (width 4). Files use **CRLF** line endings and UTF-8.
- Allman braces: the opening brace goes on its own line for functions, classes, control statements and namespaces.
- Spaces inside parentheses and no space before them: `if( x )`, `Foo( a, b )`.
- `PointerAlignment: Left` → `int* p`, `const T& v`.
- `SortIncludes: false`: keep the existing include order and don't reorder includes in files you touch.

Naming (from `.clang-tidy`):

- Types and functions: `CamelCase` (`WriteMesh`, `class CmfFile`)
- Variables and parameters: `camelBack`
- Private/member fields: `m_` prefix (`m_options`)
- Global constants: `UPPER_CASE` (`FILE_VERSION`). Globals: `g_` prefix.
- Namespaces: lowercase (`cmf`, `cmf::v1`)

Other conventions:

- Every source/CMake file starts with a copyright header: `// Copyright © <year> CCP ehf.` (or `# Copyright © ...` in CMake). New files use the current year.
- Headers use `#pragma once`.
- Warnings are errors (`COMPILE_WARNING_AS_ERROR ON`) on all targets. Fix warnings; don't suppress them. If a `NOLINT` is truly needed, scope it to a specific check, e.g. `// NOLINT(cppcoreguidelines-pro-type-vararg)`.
- Doc comments use Doxygen style (`/** ... */`, `@brief`, `@param`). All public library symbols in `include` directory must have Doxygen comments.

## Static analysis (clang-tidy)

**Run clang-tidy on every C++ change before you call it done.** Your new or modified code must produce **no clang-tidy errors or warnings**, using the checks in `.clang-tidy`.

- Scope: the existing code already has many warnings (about 2,600 at the time of writing). You must fix every warning on lines you add or modify. Don't fix warnings in code you didn't touch unless that is the task, because it bloats the diff.
- Fix the code rather than silencing the check. If a warning is a genuine false positive, use a check-specific `// NOLINT(<check-name>)` on that line and add a short reason.
- Report the result. Say which files you checked and confirm they are clean. If anything is left, list each remaining warning and explain why it stays.

### How to run it

Use `scripts/run_clang_tidy.py` from the repo root. It runs the clang-format check first, then clang-tidy:

```sh
python scripts/run_clang_tidy.py                      # all changes vs merge-base with origin/main (or main)
python scripts/run_clang_tidy.py --base HEAD          # only uncommitted changes
python scripts/run_clang_tidy.py src/cmf/writer.cpp   # limit to specific files (still changed lines only)
python scripts/run_clang_tidy.py --all-lines src/cmf/writer.cpp   # every warning in the file
python scripts/run_clang_tidy.py --fix-format         # reformat your changed lines, then run clang-tidy
python scripts/run_clang_tidy.py --format-only        # formatting check only (fast, no build)
```

**Exit code 0 means clean.** 1 means there are clang-format or clang-tidy findings, which are printed with their source context. 2 means a setup or tool failure; fix it rather than treating the check as passed.

What it does, so you don't have to:

- Finds changed C++ files under `src/` and `include/`, including committed, staged, unstaged and untracked changes. For changed headers it also checks the `.cpp` files that include them, directly or indirectly, and turns on header diagnostics for those headers.
- On first use it configures a Ninja build with a compile database in `.cmake-build-tidy/` (gitignored), using the platform's `*-release` preset. It reuses another build's `vcpkg_installed` folder when one exists, so dependencies aren't rebuilt. Later runs reuse that build. It always configures with `BUILD_TOOLS=ON`, so `cmfprocessor` and viewer sources are checked too. Use `--reconfigure` after changing CMake options. This build is only for analysis; build and test with a normal build directory (see [Build](#build)).
- On Windows it finds Visual Studio with `vswhere`, loads the `vcvars64` environment, and uses the clang-tidy, clang-format and Ninja that ship with VS, so nothing needs to be on `PATH`. Elsewhere it looks on `PATH` and in Homebrew's LLVM. Override with `--clang-tidy` / `--clang-format <path>` or the `CLANG_TIDY` / `CLANG_FORMAT` environment variables.
- Strips flags clang-tidy can't handle from the compile database: the viewer's precompiled-header flags, plus MSVC `/MP` and `/GL`.
- Reports only diagnostics on changed lines. Compiler errors are always reported, wherever they are.

Run `python scripts/run_clang_tidy.py --help` for all options (`--preset`, `--build-dir`, `--with-simplygon`, `-j`, ...).

## Architecture notes

- **File format**: data structures live in `include/cmf/v1.h` (namespace `cmf::v1`, re-exported into `cmf` by `cmf.h`). They are written to disk as-is. `cmf::Span` members become section-relative offsets on write (`PointersToOffsets`) and pointers again on load (`OffsetsToPointers`). See `doc/source/cmf/fileFormat.rst`.
  - Changing anything in `v1.h` changes the on-disk format. Treat these changes as breaking: they may need a `FILE_VERSION` bump, matching changes in the writer, `natvis`, processor `validate`/`dumpjson` and the viewer, plus doc updates. Ask before making such changes.
- **Public API**: headers in `include/cmf/` are installed. When you add a public header, list it in `PUBLIC_HEADER_FILES` in the root `CMakeLists.txt`. Add new library `.cpp` files to `CARBON_MESH_SOURCE_FILES`. Export symbols with `CARBON_MESH_EXPORT` (from the generated `carbon_mesh_export.h`).
- **cmfprocessor commands**: each subcommand is its own `.cpp` in `src/processor/` and registers itself with `REGISTER_COMMAND( "name", "description", &DescribeFn )` (see `commands.h`; `versioninfo.cpp` is a minimal example). To add a command:
  1. Create `src/processor/<name>.cpp` and add it to `PROCESSOR_SOURCES` in `src/processor/CMakeLists.txt`.
  2. Add `doc/source/cmfprocessor/<name>.rst` and link it in the toctree in `doc/source/cmfprocessor/cmfprocessor.rst`.
  3. Bump `CMF_PROCESSOR_VERSION` in `src/processor/main.cpp` if the change is user-visible.
- FBX import uses vendored **ufbx** (`src/processor/fbximport/`). Simplygon code is guarded by `#ifdef WITH_SIMPLYGON`.
- **Viewer**: sources are collected with `GLOB_RECURSE`, so new files are picked up on the next re-configure. GLSL shaders in `src/viewer/assets/shaders/` are compiled to SPIR-V headers at build time by `glslc`, and `scripts/shaderCacheCreator.py` bundles them into a cache. Shared shader code goes in `.inc` files. The viewer uses a precompiled header (`stdafx.h`).
- The project is using C++17. Using `std` library is permitted, escept `iostream`-related features in the library itself (can still be used in tools).
- Do not use `printf` or any other logging in the library, can only do that in tools.
- Do not throw or catch C++ exceptions in the library code, but can use then in tools.

## Documentation

- Add documentation for new features and update existing documentation when making changes.
- For public API provide generic descriptions with some short code samples. Do not repeat doxygen comments.
- For CLI tools, provide user guides.

## Before you finish

A C++ change is done only when **all** of these pass. In your summary, report what you ran and the result of each step. If you skipped a step, say which one and why.

1. **Format and static analysis**: `python scripts/run_clang_tidy.py` exits 0. Use `--fix-format` if formatting fails, and fix clang-tidy findings in the code.
2. **Build with no compiler warnings**: build every target your change affects, in an existing build directory configured with `BUILD_TOOLS=ON` (a preset directory or your own local one), using the configuration that matches it (see [Build](#build)). A library change affects every target. A tool change affects only that tool. Warnings are errors, so a successful build means no warnings. The glslc `GL_KHR_vulkan_glsl` warning is the known exception.
3. **Tests**: for library changes, add or update tests in `src/cmf/test/` and run `ctest --test-dir <build-dir> -C <config>`. All tests must pass.
4. **Related updates**:
   - Docs in `doc/source/` for new or changed features (see [Documentation](#documentation)).
   - New source files added to their `CMakeLists.txt` lists.
   - `CMF_PROCESSOR_VERSION` bumped for user-visible `cmfprocessor` changes.
   - Ask before any change to the on-disk format (`v1.h`).

## Workflow

- Branch from `main`.
- Keep changes focused, and don't reformat code you aren't changing.
- Go through [Before you finish](#before-you-finish) before you call a C++ change done.
- Don't edit `.teamcity/` (CI configuration) unless that is the task.
- For contribution rules (CLA, PR template), see the [Carbon Engine contributing guide](https://github.com/carbonengine/.github/blob/main/CONTRIBUTING.md).
