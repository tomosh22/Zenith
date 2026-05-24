// Hand-written replacement for the cmake-generated msdfgen-config.h.
// CMake's configure_file substitutes ${MSDFGEN_VERSION} etc. from cmake/version.cmake;
// we replicate the resolved values directly so the build doesn't depend on cmake.
// Values must agree with the vendored msdfgen version (vcpkg.json: "1.12.0").
//
// Included from core/base.h as <msdfgen/msdfgen-config.h>. The include path
// Tools/Middleware/msdf-atlas-gen/msdfgen/Config is added to Sharpmake so that
// the <msdfgen/...> prefix resolves to this directory.
#pragma once

#define MSDFGEN_PUBLIC          // static lib build: empty visibility macro
#define MSDFGEN_EXT_PUBLIC      // same for ext targets

#define MSDFGEN_VERSION          "1.12.0"
#define MSDFGEN_VERSION_MAJOR    1
#define MSDFGEN_VERSION_MINOR    12
#define MSDFGEN_VERSION_REVISION 0
#define MSDFGEN_COPYRIGHT_YEAR   2024

// Feature flags. Matches the Zenith build's intent: minimal deps, no SVG/PNG.
#define MSDFGEN_USE_CPP11
#define MSDFGEN_EXTENSIONS
#define MSDFGEN_DISABLE_SVG
#define MSDFGEN_DISABLE_PNG
