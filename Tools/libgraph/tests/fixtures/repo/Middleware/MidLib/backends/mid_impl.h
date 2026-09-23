#pragma once

// Scenario 20: duplicate. Listed by BOTH LibB's and LibC's vcxproj (mirrors
// the real imgui-docking backend files listed by both the Vulkan backend
// and Windows projects). Must be owned by MidLib (the Entry-table
// middleware node whose source root "Middleware/MidLib" prefix-matches this
// path) and reported in Model.duplicates.
void MidLib_BackendImpl();
