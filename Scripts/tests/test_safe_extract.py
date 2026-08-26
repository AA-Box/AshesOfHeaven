"""Traversal check for the archive extraction in Scripts/ImportAnimationDrop.py.

    python3 Scripts/tests/test_safe_extract.py

The module imports `unreal`, which only exists inside the editor, so it is stubbed. What is
under test is pure path logic: an archive member name is attacker-controlled data and must not
be able to write outside the destination.
"""

import io
import os
import sys
import tarfile
import tempfile
import zipfile
from unittest.mock import MagicMock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# A permissive stub: the module calls unreal APIs at import time (AssetToolsHelpers,
# EditorAssetLibrary), and none of them matter to the path logic under test.
sys.modules["unreal"] = MagicMock()
sys.path.insert(0, ROOT)

import importlib.util

spec = importlib.util.spec_from_file_location(
    "import_animation_drop", os.path.join(ROOT, "ImportAnimationDrop.py"))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def test_safe_join():
    with tempfile.TemporaryDirectory() as root:
        for hostile in ("../evil", "../../evil", "/etc/evil", "a/../../evil",
                        "./../evil", "", "..", "a/../..//evil"):
            assert mod._safe_join(root, hostile) is None, hostile
        for ok in ("a/b.fbx", "Assets/Art/x.fbx", "./a/b.fbx"):
            resolved = mod._safe_join(root, ok)
            assert resolved is not None, ok
            assert mod._contained(root, resolved), ok


def test_tar_traversal_is_not_written():
    with tempfile.TemporaryDirectory() as work:
        archive = os.path.join(work, "a.tar.gz")
        root = os.path.join(work, "out")
        os.makedirs(root)
        with tarfile.open(archive, "w:gz") as tar:
            for name in ("../escaped.txt", "good.txt"):
                data = b"x"
                info = tarfile.TarInfo(name)
                info.size = len(data)
                tar.addfile(info, io.BytesIO(data))
            link = tarfile.TarInfo("link")
            link.type = tarfile.SYMTYPE
            link.linkname = "/etc/passwd"
            tar.addfile(link)
        assert mod.extract_tar_gz(archive, root) == 1
        assert os.path.isfile(os.path.join(root, "good.txt"))
        assert not os.path.exists(os.path.join(work, "escaped.txt"))
        assert not os.path.lexists(os.path.join(root, "link"))


def test_zip_traversal_is_not_written():
    with tempfile.TemporaryDirectory() as work:
        archive = os.path.join(work, "a.zip")
        root = os.path.join(work, "out")
        os.makedirs(root)
        with zipfile.ZipFile(archive, "w") as zf:
            zf.writestr("../escaped.txt", "x")
            zf.writestr("nested/good.txt", "x")
        assert mod.extract_zip(archive, root) == 1
        assert os.path.isfile(os.path.join(root, "nested", "good.txt"))
        assert not os.path.exists(os.path.join(work, "escaped.txt"))


if __name__ == "__main__":
    mod.REPORT = []
    test_safe_join()
    test_tar_traversal_is_not_written()
    test_zip_traversal_is_not_written()
    print("safe extraction: 3 checks passed")
