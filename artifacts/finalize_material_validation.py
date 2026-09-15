from pathlib import Path
import json, re, hashlib

root = Path('C:/dev/Zenith')
out = root/'artifacts/signpost-multimaterial'
baselines = json.loads((root/'Tools/unit_baselines.json').read_text())['baselines']
suites = {}
for game in ['Combat', 'RenderTest', 'Zenithmon']:
    log = (out/f'{game}-units.log').read_text(encoding='utf-8')
    counts = re.findall(r'Unit tests complete: (\d+) ran, (\d+) passed, (\d+) failed, (\d+) skipped', log)
    assert counts, game
    ran, passed, failed, skipped = map(int, counts[-1])
    assert ran == baselines[game] and failed == 0 and passed + skipped == ran, (game, counts[-1])
    for test in ['ModelMaterialSlotsAreLocalToEachMeshBinding', 'ImportedMaterialSectionsKeepTheirOwnIndexRanges', 'MeshDrawSectionsRejectInvalidRangesAndKeepLegacyMeshes']:
        assert test in log, (game, test)
    suites[game] = dict(registered=ran, passed=passed, failed=failed, skipped=skipped)
showcase = json.loads((out/'ZM_ImportedPropShowcase_Test.json').read_text())
assert showcase['passed'] and not showcase['failures']
asset = json.loads((out/'asset-verification.json').read_text())
glb = root/'Games/Zenithmon/Assets/Props/SignPost/SignPost.glb'
assert hashlib.sha256(glb.read_bytes()).hexdigest() == asset['sha256']
report = {'asset': asset, 'unitSuites': suites, 'graphicsShowcase': showcase,
          'screenshots': sorted(p.name for p in out.glob('prop_signpost*.png')),
          'inWorldIndexRanges': [[0, 1980], [1980, 14304]]}
(out/'validation.json').write_text(json.dumps(report, indent=2))
print(json.dumps({'unitSuites': suites, 'graphicsShowcase': showcase}, indent=2))
