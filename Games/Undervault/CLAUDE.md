# Undervault visual studies
Presentation only: three fixed 3D cutaways, posed Vaulters, decorative HUDs.
No gameplay components, simulation, input, save systems or working networks.
Preserve Docs and all reference PNGs. Work in this existing worktree/branch.
SourceArt/generate_mockups.py deterministically generates the editable Blender
scenes and GLBs. SourceArt/generate_huds.ps1 authors transparent HUD artwork.
UNDERVAULT_MOCKUP selects Colony (default), Planning or Flooding at boot.
Normal tools boot imports assets and bakes the selected scene; runtime loads it.
Ambient particles, if used, must use Flux GPU compute with CPU particles disabled.
