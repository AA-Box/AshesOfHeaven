import unreal
import os

"""One-boot driver for the Phase 4.7 Erebus iteration: textures -> PBR master ->
kit (materials + heroes) -> sprite materials -> level recompose."""

SCRIPTS = [
    "ImportErebusTextures.py",
    "AuthorErebusPBRMasters.py",
    "GenerateErebusArtKit.py",
    "AuthorVFXSpriteMaterials.py",
    "BuildErebusOpeningLevel.py",
]

root = os.path.join(unreal.Paths.project_dir(), "Scripts")
for name in SCRIPTS:
    path = os.path.join(root, name)
    unreal.log("[Iter2] ==== running %s ====" % name)
    with open(path) as handle:
        code = handle.read()
    exec(compile(code, path, "exec"), {"__name__": "__main__", "__file__": path})
unreal.log("[Iter2] ALL SCRIPTS DONE")
