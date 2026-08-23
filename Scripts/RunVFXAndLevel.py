import unreal, os
for name in ["AuthorVFXSpriteMaterials.py", "BuildErebusOpeningLevel.py"]:
    path = os.path.join(unreal.Paths.project_dir(), "Scripts", name)
    with open(path) as handle:
        code = handle.read()
    exec(compile(code, path, "exec"), {"__name__": "__main__", "__file__": path})
