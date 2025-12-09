#pragma once

#ifdef ZENITH_TOOLS

#include "Core/Zenith_Core.h"

// Enable verbose editor logging for debugging
// Comment out to disable detailed logs
// #define ZENITH_EDITOR_VERBOSE_LOGGING

#ifdef ZENITH_EDITOR_VERBOSE_LOGGING
    #define EDITOR_LOG_SELECTION(fmt, ...) Zenith_Log("[Editor/Selection] " fmt, ##__VA_ARGS__)
    #define EDITOR_LOG_GIZMO(fmt, ...) Zenith_Log("[Editor/Gizmo] " fmt, ##__VA_ARGS__)
    #define EDITOR_LOG_PROPERTY(fmt, ...) Zenith_Log("[Editor/Property] " fmt, ##__VA_ARGS__)
    #define EDITOR_LOG_SCENE(fmt, ...) Zenith_Log("[Editor/Scene] " fmt, ##__VA_ARGS__)
#else
    #define EDITOR_LOG_SELECTION(fmt, ...) ((void)0)
    #define EDITOR_LOG_GIZMO(fmt, ...) ((void)0)
    #define EDITOR_LOG_PROPERTY(fmt, ...) ((void)0)
    #define EDITOR_LOG_SCENE(fmt, ...) ((void)0)
#endif

#endif // ZENITH_TOOLS
