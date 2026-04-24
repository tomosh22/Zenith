#!/usr/bin/env python3
"""
Post-migration pass: re-categorize ZENITH_TEST(Core, Xxx) entries based on
the test-name prefix so tests appear under meaningful categories.

Usage:
    python recategorize_tests.py <file.cpp>

Renaming is in-place. Only `ZENITH_TEST(Core, Name)` entries are rewritten
(so running this twice is a no-op).
"""
import re
import sys
from pathlib import Path

# Ordered list of (prefix, category). The first matching prefix wins.
# Check longer-specific prefixes before generic ones (e.g. TestBlendSpace
# before TestBlend).
RULES = [
    # Scene / ECS
    ('Scene',                      'Scene'),
    ('Query',                      'ECS'),
    ('Component',                  'ECS'),
    ('Entity',                     'ECS'),
    ('Event',                      'ECS'),
    ('Lifecycle',                  'ECS'),
    ('Prefab',                     'Prefab'),

    # Animation-related
    ('AnimationClipChannels',      'Animation'),
    ('AnimationController',        'Animation'),
    ('AnimationLayer',             'Animation'),
    ('AnimationParameters',        'Animation'),
    ('AnimationSerialization',     'Animation'),
    ('AnimationStateMachine',      'Animation'),
    ('AnimatorComponent',          'Animation'),
    ('BlendSpace',                 'Animation'),
    ('BlendTree',                  'Animation'),
    ('Bone',                       'Animation'),
    ('CrossFade',                  'Animation'),
    ('IK',                         'Animation'),
    ('Skeleton',                   'Animation'),
    ('StateMachine',               'Animation'),
    ('Transition',                 'Animation'),
    ('Layer',                      'Animation'),

    # Assets
    ('AssetHandle',                'Asset'),
    ('AssetRegistry',              'Asset'),
    ('AsyncLoad',                  'Asset'),
    ('AsyncAsset',                 'Asset'),
    ('DataAsset',                  'Asset'),
    ('MaterialAsset',              'Asset'),
    ('MeshAsset',                  'Asset'),
    ('ModelAsset',                 'Asset'),
    ('ModelInstance',              'Asset'),
    ('SkeletonAsset',              'Asset'),
    ('AnimationAsset',             'Asset'),

    # Rendering subsystems
    ('ImageView',                  'Vulkan'),
    ('Destroy',                    'Vulkan'),
    ('Gizmos',                     'Gizmos'),
    ('ChunkDistance',              'Terrain'),
    ('Slang',                      'Slang'),
    ('UIStyle',                    'UI'),

    # Tweens
    ('Tween',                      'Tween'),
    ('Easing',                     'Tween'),

    # Core containers / utilities
    ('CircularQueue',              'Core'),
    ('DataStream',                 'Core'),
    ('HashMap',                    'Core'),
    ('HashSet',                    'Core'),
    ('MemoryManagement',           'Core'),
    ('MemoryPool',                 'Core'),
    ('Profiling',                  'Core'),
    ('Vector',                     'Core'),
]


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    path = Path(sys.argv[1])
    text = path.read_text(encoding='utf-8')

    pattern = re.compile(r'ZENITH_TEST\(\s*Core\s*,\s*(\w+)\s*\)')

    def repl(m: re.Match) -> str:
        name = m.group(1)
        for prefix, cat in RULES:
            if name.startswith(prefix):
                return f'ZENITH_TEST({cat}, {name})'
        return m.group(0)  # no rule matched, leave as Core

    new_text, count = pattern.subn(repl, text)
    path.write_text(new_text, encoding='utf-8')

    # Count categories.
    cat_counts = {}
    for m in re.finditer(r'ZENITH_TEST\(\s*(\w+)\s*,', new_text):
        cat_counts[m.group(1)] = cat_counts.get(m.group(1), 0) + 1
    print(f"Processed: {path}")
    for cat in sorted(cat_counts):
        print(f"  {cat}: {cat_counts[cat]}")


if __name__ == '__main__':
    main()
