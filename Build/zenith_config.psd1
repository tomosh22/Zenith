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

    # HEADLESS config. Headless is BUILD-TIME (there is no --headless runtime
    # flag): a Null_* config defines ZENITH_NULL_RENDERER, compiles the GPU-less
    # Zenith/Null backend and creates its window hidden. Every headless batch,
    # unit gate and CI run executes THIS exe. Tools=True so tools-only asset
    # authoring / test steps still compile.
    HeadlessConfigWin64   = 'Null_vs2022_Debug_Win64_True'

    # Config used to build + locate the ZenithHub launcher exe.
    HubConfigWin64        = 'Vulkan_vs2022_Release_Win64_False'

    # Android native-lib OUTPUT DIRECTORY template
    # ({0} = ABI config token 'arm64_v8a' | 'x86_64', {1} = 'debug' | 'release').
    #
    # This is the OutDir leaf that Sharpmake pins via CustomProperties in
    # Sharpmake_Games.cs -- NOT the MSBuild /p:Configuration name. The two
    # differ deliberately: the MSBuild config IS prefixed (e.g.
    # 'Vulkan_x86_64_vs2022_Debug_Agde_False', platform 'Android-x86_64'),
    # while this pinned OutDir omits the backend prefix. Use
    # AndroidMsBuildConfigTemplate below when you need the config name.
    AndroidOutDirTemplate       = '{0}_vs2022_{1}_agde_false'

    # Android MSBuild /p:Configuration template ({0} = ABI config token,
    # {1} = 'Debug' | 'Release'). Paired platform is 'Android-<abi dir name>'
    # ('Android-arm64-v8a' / 'Android-x86_64') -- note the dash spelling.
    AndroidMsBuildConfigTemplate = 'Vulkan_{0}_vs2022_{1}_Agde_False'

    # The ABI axis, mirroring ZenithAndroidAbi.All (Build/Sharpmake_Common.cs)
    # and Build/zenith_android_abis.gradle. x86_64 is the emulator ABI.
    #
    #   Key = config token ('arm64_v8a')      Value = ABI dir name ('arm64-v8a')
    #
    # CAUTION: this is the TRANSPOSE of the map in zenith_android_abis.gradle,
    # which is keyed the other way round (dir name -> token) because that is what
    # Gradle's abiFilters wants. Do NOT copy a pair across verbatim when adding an
    # ABI -- swap it. The two spellings coincide for x86_64, so an inverted entry
    # for it looks fine and only breaks on the next arm-like ABI. Read this via
    # Get-ZenithAndroidAbis, which hands back both spellings as named fields so no
    # call site ever has to know which way round the storage is.
    AndroidAbis                 = @{ 'arm64_v8a' = 'arm64-v8a'; 'x86_64' = 'x86_64' }

    # Middleware versions CI provisions (the vendored compile-time SDK under
    # Middleware/VulkanSDK/ is versioned by its directory name, not here).
    SlangVersion          = '2026.1'
    VulkanSdkVersion      = '1.3.290.0'

    # Canonical untracked artifact root (gitignored). Runner scripts derive
    # their output dirs from this; see AGENTS.md artifact-root rule.
    ArtifactsRoot         = 'Build/artifacts'
}
