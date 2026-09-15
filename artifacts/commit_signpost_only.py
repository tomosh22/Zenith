from pathlib import Path
import subprocess, os, hashlib, json, sys

root = Path('C:/dev/Zenith')
meta = root/'artifacts/signpost-commit-state.json'
index = root/'artifacts/signpost-commit.index'
paths = [
    'Games/Zenithmon/Assets/Scenes/Dawnmere.zscen',
    'Games/Zenithmon/Docs/ArtBrief.md',
    'Games/Zenithmon/Source/World/ZM_DawnmereDressing.h',
    'Games/Zenithmon/Tests/ZM_AutoTests_ImportedPropShowcase.cpp',
    'Games/Zenithmon/Zenithmon.cpp',
    'Tools/unit_baselines.json',
    'Tools/Art/README.md',
    'Tools/Art/create_signpost.py',
    'Zenith/Flux/Flux_GPUScene.Tests.inl',
    'Zenith/Flux/Flux_GPUScene.cpp',
    'Zenith/Flux/Flux_GPUScene.h',
    'Zenith/Flux/Flux_GPUSceneBuilder.cpp',
    'Zenith/Flux/Flux_ModelInstance.cpp',
    'Zenith/Flux/Flux_ModelInstance.h',
    'Zenith/Flux/Shaders/UnifiedMesh/Flux_UnifiedMesh_Reset.slang',
    'Zenith/Flux/Translucency/Flux_Translucency.cpp',
    'Zenith/Flux/Translucency/Flux_TranslucencyImpl.h',
    'Zenith/Flux/UnifiedMesh/Flux_UnifiedMesh.cpp',
    'Zenith/Flux/UnifiedMesh/Flux_UnifiedMeshImpl.h',
]
env = dict(os.environ, GIT_INDEX_FILE=str(index))
def git(*args, alternate=False, data=None):
    return subprocess.check_output(['git', *args], cwd=root, env=env if alternate else None, input=data)
def digest(data):
    return hashlib.sha256(data).hexdigest()
def scripting_state():
    names = git('ls-files', '-z', '--', 'Zenith/Scripting').decode().split('\0')
    return {'index': digest(git('ls-files', '-s', '--', 'Zenith/Scripting')),
            'working': {p: digest((root/p).read_bytes()) for p in names if p}}

if sys.argv[1] == 'prepare':
    assert not index.exists() and not meta.exists(), 'Commit preparation already exists'
    state = {'head': git('rev-parse', 'HEAD').decode().strip(),
             'scripting': scripting_state(),
             'baselineWorking': digest((root/'Tools/unit_baselines.json').read_bytes()),
             'paths': paths}
    git('read-tree', 'HEAD', alternate=True)
    git('add', '--', *[p for p in paths if p != 'Tools/unit_baselines.json'], alternate=True)
    base = git('show', 'HEAD:Tools/unit_baselines.json').decode()
    baseline = json.loads(base)['baselines']
    working = json.loads((root/'Tools/unit_baselines.json').read_text())['baselines']
    for game in ['Zenithmon', 'Combat', 'RenderTest']:
        assert working[game] - baseline[game] == 47, (game, baseline[game], working[game])
        base = base.replace(f'"{game}": {baseline[game]}', f'"{game}": {baseline[game]+3}')
    blob = git('hash-object', '-w', '--stdin', data=base.encode()).decode().strip()
    git('update-index', '--cacheinfo', '100644', blob, 'Tools/unit_baselines.json', alternate=True)
    changed = git('diff', '--cached', '--name-only', alternate=True).decode().splitlines()
    assert set(changed) == set(paths), changed
    assert scripting_state() == state['scripting']
    meta.write_text(json.dumps(state, indent=2))
    print(git('diff', '--cached', '--stat', alternate=True).decode())
    print(git('diff', '--cached', '--', 'Tools/unit_baselines.json', alternate=True).decode())
    print('Separate index prepared. Real Scripting index and working files unchanged.')
elif sys.argv[1] == 'commit':
    state = json.loads(meta.read_text())
    assert git('rev-parse', 'HEAD').decode().strip() == state['head']
    assert scripting_state() == state['scripting']
    assert digest((root/'Tools/unit_baselines.json').read_bytes()) == state['baselineWorking']
    git('diff', '--cached', '--check', alternate=True)
    body = root/'artifacts/signpost-commit-message.txt'
    body.write_text('Fix imported mesh material sections and author Dawnmere signpost\n\n'
        'Render each mesh section with its local material and index range, sharing geometry across opaque, skinned, shadow and supported translucent draws. Preserve per-binding material offsets.\n\n'
        'Author the signpost with separate oak and iron PBR materials, integrate both Dawnmere placements, and add renderer and in-world regression coverage. Retain the reproducible Blender script; source-art binaries remain gitignored.\n\n'
        'Validation: Vulkan showcase passed; Null suites completed with zero failures in Zenithmon, Combat and RenderTest. These runs included preexisting local Scripting work; this commit excludes it and includes only the three new renderer tests in the baseline delta.\n')
    print(git('commit', '-F', str(body), alternate=True).decode())
    # Update only our entries in the real index. The working files are untouched,
    # including the baseline's preexisting +44, and Scripting stays staged.
    git('reset', '-q', 'HEAD', '--', *paths)
    assert scripting_state() == state['scripting'], 'Scripting changed unexpectedly'
    assert digest((root/'Tools/unit_baselines.json').read_bytes()) == state['baselineWorking']
    committed = git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines()
    assert set(committed) == set(paths), committed
    print(git('rev-parse', 'HEAD').decode().strip())
    print('Verified: Scripting contents and staged entries unchanged; baseline working file unchanged.')
elif sys.argv[1] == 'verify':
    state = json.loads(meta.read_text())
    assert scripting_state() == state['scripting']
    assert digest((root/'Tools/unit_baselines.json').read_bytes()) == state['baselineWorking']
    print('Preexisting Scripting work and baseline remainder preserved.')
