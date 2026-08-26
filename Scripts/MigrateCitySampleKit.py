#!/usr/bin/env python3
"""Copy the CitySample city art kit into this project, preserving /Game paths.

Only the art both CitySample maps actually reference is taken: no maps, no external
actors, no crowd, vehicles, characters, gameplay framework or input. Roughly 6.5k
packages / 30 GB. The copied folders are gitignored - this script is how they come
back, so it takes no manual file list.

Package references are read straight out of the uasset name table rather than from
the asset registry, because CitySample's own C++ module does not load against this
engine install and its editor therefore cannot be opened here.

    python3 Scripts/MigrateCitySampleKit.py [--dry-run]

Source project: $AH_CITYSAMPLE (default ~/Documents/Unreal Projects/CitySample).
Nothing in the source is modified, and an existing destination file is never
overwritten - a collision is reported and skipped.
"""
import argparse
import os
import re
import shutil
import sys

SRC_PROJECT = os.environ.get(
    "AH_CITYSAMPLE", os.path.expanduser("~/Documents/Unreal Projects/CitySample"))
DST_PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Roots whose closure defines the kit. Both city maps are scanned for what they use;
# the maps and their external actors are then dropped, leaving only the art.
ROOT_MAPS = ["/Game/Map/Small_City_LVL", "/Game/Map/Big_City_LVL"]
ROOT_EXTERNAL_ACTOR_DIRS = [
    "__ExternalActors__/Map/Small_City_LVL",
    "__ExternalActors__/Map/Big_City_LVL",
]
# Dropped from the closure: CitySample gameplay and everything that needs its C++
# module or simulates a living city. A ruined Erebus district wants none of it.
EXCLUDE = (
    "/Game/Crowd/", "/Game/Vehicle/", "/Game/Character/", "/Game/AI/",
    "/Game/Cinematics/", "/Game/UI/", "/Game/Audio/", "/Game/Gameplay/",
    "/Game/Input/", "/Game/__ExternalActors__/", "/Game/Map/",
)
# One vehicle material function lives in the Traffic plugin. Traffic is a C++ plugin
# and building it here would drag in Mass; the single asset is republished as a
# content-only plugin instead so the /Traffic/ mount point still resolves.
# Two shared assets live under excluded folders but are referenced by kit materials;
# without them those materials fail to compile. Pulled in explicitly.
EXTRA_PACKAGES = (
    "/Game/Crowd/Character/Shared/MaterialFunctions/MF_NormalStrength",
    "/Game/Crowd/Character/Shared/Textures/Body/WhiteSquareTexture",
)
# CitySample's doppelganger/dissolve demo FX are the only kit content still pointing at
# the excluded character and vehicle art. Nothing in a ruined city needs them, so they are
# dropped from the closure and any tree an earlier run copied is removed.
PRUNE = (
    "/Game/Effect/Vehicle/",
    "/Game/Effect/Niagara/Doppelganger/",
    "/Game/Effect/Niagara/Dissolve/",
    "/Game/Effect/World/DissolveEffect/",
    "/Game/Effect/UI/UI/Materials/Function/DigitalText_Layer",
)

CONTENT_ONLY_PLUGIN = """{
	"FileVersion": 3,
	"FriendlyName": "Traffic",
	"Description": "Content-only stub carrying the CitySample Traffic material functions the migrated city art references.",
	"Category": "Other",
	"CanContainContent": true,
	"Installed": false
}
"""

REFERENCE = re.compile(rb"/[A-Za-z0-9_]+(?:/[A-Za-z0-9_\-.]+)+")


def mount_points(project):
    mounts = {"/Game/": os.path.join(project, "Content")}
    plugins = os.path.join(project, "Plugins")
    if os.path.isdir(plugins):
        for name in sorted(os.listdir(plugins)):
            content = os.path.join(plugins, name, "Content")
            if os.path.isdir(content):
                mounts["/" + name + "/"] = content
    return mounts


def package_file(mounts, package):
    for mount, root in mounts.items():
        if package.startswith(mount):
            base = os.path.join(root, package[len(mount):])
            for ext in (".uasset", ".umap"):
                if os.path.isfile(base + ext):
                    return base + ext
            return None
    return None


def referenced_packages(path):
    with open(path, "rb") as handle:
        data = handle.read()
    found = set()
    for match in REFERENCE.finditer(data):
        name = match.group().decode("ascii", "ignore").split(".")[0]
        if name.startswith(("/Script/", "/Engine/", "/Temp/")):
            continue
        found.add(name)
    return found


def packages_under(content_root, relative):
    out = []
    for dirpath, _, names in os.walk(os.path.join(content_root, relative)):
        for name in names:
            if name.endswith((".uasset", ".umap")):
                full = os.path.join(dirpath, name)
                out.append("/Game/" + os.path.relpath(full, content_root).rsplit(".", 1)[0])
    return out


def closure(mounts, roots):
    seen, stack, resolved = set(), list(roots), {}
    while stack:
        package = stack.pop()
        if package in seen:
            continue
        seen.add(package)
        path = package_file(mounts, package)
        if path is None:
            continue
        resolved[package] = path
        for reference in referenced_packages(path):
            if reference not in seen:
                stack.append(reference)
    return resolved


def verify():
    """Every reference the copied kit makes must resolve here, or the closure missed something."""
    source_mounts = mount_points(SRC_PROJECT)
    dest_mounts = mount_points(DST_PROJECT)
    content = dest_mounts["/Game/"]
    gaps = {}
    scanned = 0
    # The project's own authored content is not part of the kit and is not checked here.
    own = ("Ashes", "ChapterOne", "Characters", "Combat", "FirstPerson", "Input",
           "LevelPrototyping", "Weapons", "__ExternalActors__", "__ExternalObjects__")
    for top in sorted(os.listdir(content)):
        root = os.path.join(content, top)
        if top in own or not os.path.isdir(root):
            continue
        for dirpath, _, names in os.walk(root):
            for name in names:
                if not name.endswith((".uasset", ".umap")):
                    continue
                scanned += 1
                for reference in referenced_packages(os.path.join(dirpath, name)):
                    if not reference.startswith("/Game/"):
                        continue
                    if package_file(dest_mounts, reference) is not None:
                        continue
                    if package_file(source_mounts, reference) is None:
                        continue  # not a real CitySample package, just a byte-pattern match
                    if reference.startswith(EXCLUDE + PRUNE):
                        continue  # deliberately dropped
                    gaps[reference] = gaps.get(reference, 0) + 1
    print("verified %d files, %d unresolved dependencies" % (scanned, len(gaps)))
    for reference, count in sorted(gaps.items(), key=lambda item: -item[1])[:20]:
        print("  %6d  %s" % (count, reference))
    return 1 if gaps else 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--verify", action="store_true",
                        help="report references the copied kit makes that this project cannot "
                             "resolve but CitySample can - i.e. dependencies the closure missed")
    args = parser.parse_args()

    if args.verify:
        return verify()

    if not os.path.isdir(SRC_PROJECT):
        sys.exit("CitySample not found at %s (set AH_CITYSAMPLE)" % SRC_PROJECT)

    mounts = mount_points(SRC_PROJECT)
    src_content = mounts["/Game/"]
    roots = list(ROOT_MAPS)
    for relative in ROOT_EXTERNAL_ACTOR_DIRS:
        roots += packages_under(src_content, relative)
    print("scanning %d roots" % len(roots))

    resolved = closure(mounts, roots)
    kit = {p: f for p, f in resolved.items()
           if not p.startswith(EXCLUDE) and not p.startswith(PRUNE)}
    for package in EXTRA_PACKAGES:
        path = package_file(mounts, package)
        if path:
            kit[package] = path
    total = sum(os.path.getsize(f) for f in kit.values())
    print("kit: %d packages, %.2f GB" % (len(kit), total / 2 ** 30))

    dst_mounts = mount_points(DST_PROJECT)
    dst_mounts.setdefault("/Traffic/", os.path.join(DST_PROJECT, "Plugins", "Traffic", "Content"))

    copied = skipped = collided = 0
    for package, src in sorted(kit.items()):
        for mount, root in dst_mounts.items():
            if package.startswith(mount):
                dst = os.path.join(root, package[len(mount):]) + os.path.splitext(src)[1]
                break
        else:
            collided += 1
            print("  no mount point for %s" % package)
            continue
        if os.path.exists(dst):
            skipped += 1
            continue
        if args.dry_run:
            copied += 1
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
        if copied % 500 == 0:
            print("  %d/%d" % (copied, len(kit)))

    uplugin = os.path.join(DST_PROJECT, "Plugins", "Traffic", "Traffic.uplugin")
    if not args.dry_run and not os.path.exists(uplugin):
        os.makedirs(os.path.dirname(uplugin), exist_ok=True)
        with open(uplugin, "w") as handle:
            handle.write(CONTENT_ONLY_PLUGIN)

    for package in PRUNE:
        victim = os.path.join(DST_PROJECT, "Content", package[len("/Game/"):].rstrip("/"))
        if args.dry_run:
            continue
        if os.path.isdir(victim):
            shutil.rmtree(victim)
            print("pruned %s" % package)
        elif os.path.isfile(victim + ".uasset"):
            os.remove(victim + ".uasset")
            print("pruned %s" % package)

    print("copied %d, already present %d, unmounted %d" % (copied, skipped, collided))


if __name__ == "__main__":
    raise SystemExit(main() or 0)
