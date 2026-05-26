#pragma once

// This header previously declared:
//   - class Zenith_EditorPanel: an abstract base class for editor panels
//   - namespace Zenith_EditorPanels: transitional factory-style render functions
// Both were removed because they had no derived classes / no implementations and
// no callers anywhere in the repo. The actual editor panels are implemented as
// per-feature namespaces (Zenith_EditorPanelHierarchy, ...PanelProperties, etc.)
// in sibling Zenith_EditorPanel_*.{h,cpp} files.
//
// The file itself is kept (now empty) so that the Sharpmake-generated
// zenith_win64.vcxproj reference does not need to be regenerated. A future pass
// can remove the file outright once Sharpmake is re-run.
