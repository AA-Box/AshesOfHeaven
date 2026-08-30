#!/usr/bin/env python3
"""Copy the Teuthisan alien character into this project, preserving its /Game path.

The character is a self-contained folder in another project, so unlike the CitySample kit
there is no closure to compute: everything it references lives under the one directory, and
copying it to the SAME package path (/Game/Characters/Teuthisan) is what makes its internal
references resolve with zero redirectors. That path is load-bearing - copying it to
/Game/Ashes/Enemies/... instead would break every material and texture reference inside it.

The folder is gitignored, the way the CitySample kit is: 871 MB (a 195 MB skeletal mesh and
about 690 MB of per-zone textures) is far past what a repo with no LFS can carry. This script
is the restore path.

    python3 Scripts/MigrateTeuthisan.py [--dry-run]

After migrating, the derived game assets are rebuilt in this order (all UnrealEditor-Cmd):
BakeTeuthisanTakes.py (cinematic takes -> AnimSequences), AuthorTeuthisanAnimations.py (the
five gameplay clips), PrepareTeuthisanGameMesh.py (Nanite + the authored LOD chain), then
AuthorEnemyDefinitions.py to re-author DA_Enemy_Teuthisan against the measured mesh.

Source project: $AH_TEUTHISAN (default ~/Documents/Unreal Projects/ASCTeuthisan). The source
project is UE 5.3 and this one is 5.8; the packages upgrade on load, which costs a slow first
open and nothing after that. Nothing in the source is modified.
"""
import argparse
import os
import shutil
import sys

SRC_PROJECT = os.environ.get(
    "AH_TEUTHISAN", os.path.expanduser("~/Documents/Unreal Projects/ASCTeuthisan"))
DST_PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RELATIVE = os.path.join("Content", "Characters", "Teuthisan")

# What the folder is expected to contain, as a guard against a half-copied or moved source.
# Named individually because a missing skeletal mesh and a missing texture fail very
# differently: one is an obvious empty asset, the other is a silently grey character.
EXPECTED = (
    "Rig/SKM_Teuthisan_rig_v001.uasset",
    "Rig/SK_Teuthisan_rig_v001.uasset",
    "Rig/PA_Teuthisan_rig_v001.uasset",
    "Rig/CR_Teuthisan_rig_v001.uasset",
    "Animations/LS_Teuthisan_Idle.uasset",
    "Animations/LS_Teuthisan_Crawl.uasset",
    "Animations/LS_Teuthisan_Stand.uasset",
    "Animations/LS_Teuthisan_Death.uasset",
    "Materials/M_Alien_Base.uasset",
)
MIN_TOTAL_BYTES = 800 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    source = os.path.join(SRC_PROJECT, RELATIVE)
    destination = os.path.join(DST_PROJECT, RELATIVE)
    if not os.path.isdir(source):
        sys.exit("source not found: %s (set AH_TEUTHISAN)" % source)

    missing = [name for name in EXPECTED if not os.path.isfile(os.path.join(source, name))]
    if missing:
        sys.exit("source is incomplete, missing: %s" % ", ".join(missing))

    total = 0
    files = []
    for root, _dirs, names in os.walk(source):
        for name in names:
            if name.startswith("."):
                continue
            path = os.path.join(root, name)
            total += os.path.getsize(path)
            files.append(path)
    if total < MIN_TOTAL_BYTES:
        sys.exit("source is only %.0f MB, expected at least %.0f MB - wrong folder?"
                 % (total / 1e6, MIN_TOTAL_BYTES / 1e6))

    print("%d files, %.0f MB" % (len(files), total / 1e6))
    print("%s\n  -> %s" % (source, destination))
    if args.dry_run:
        return

    copied = skipped = 0
    for path in files:
        relative = os.path.relpath(path, source)
        target = os.path.join(destination, relative)
        # Never overwrite: a destination file that already differs is either a newer
        # migration or local work, and silently clobbering it is how a day disappears.
        if os.path.isfile(target):
            skipped += 1
            continue
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(path, target)
        copied += 1
    print("copied %d, skipped %d already present" % (copied, skipped))


if __name__ == "__main__":
    main()
