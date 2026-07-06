# zenith_config.psd1
# =============================================================================
# Central build-system configuration data. Pure data (Import-PowerShellDataFile),
# no code. Read via Get-ZenithBuildConfigData in Build/zenith_buildsystem.psm1;
# never Import-PowerShellDataFile this from call sites directly.
#
# Single source of truth for values that were previously hardcoded across the
# zenith CLI, test runners, deploy scripts, and (eventually) CI workflows.
# =============================================================================
@{
    # Default MSBuild /p:Configuration for zenith build / run / test.
    DefaultConfigWin64    = 'Vulkan_vs2022_Debug_Win64_True'

    # Config used to build + locate the ZenithHub launcher exe.
    HubConfigWin64        = 'Vulkan_vs2022_Release_Win64_False'

    # Android native-lib config name template ({0} = 'debug' | 'release').
    AndroidConfigTemplate = 'arm64_v8a_vs2022_{0}_agde_false'

    # Middleware versions CI provisions (the vendored compile-time SDK under
    # Middleware/VulkanSDK/ is versioned by its directory name, not here).
    SlangVersion          = '2026.1'
    VulkanSdkVersion      = '1.3.290.0'

    # Canonical untracked artifact root (gitignored). Runner scripts derive
    # their output dirs from this; see AGENTS.md artifact-root rule.
    ArtifactsRoot         = 'Build/artifacts'
}
