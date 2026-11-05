"""Input identities and append-only output boundaries shared by the tools."""
import hashlib
import json
from pathlib import Path
import re

from . import VERSION


def digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(value, length=64):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{%d}" % length, value),
            "invalid immutable identity")
    return value


def relative(value):
    path = Path(value)
    require(isinstance(value, str) and value and not path.is_absolute()
            and ".." not in path.parts and str(path) == value, "unsafe relative path")
    return path


def record(path, value):
    """Never replace a prior receipt, index, media file or historical assessment."""
    with Path(path).open("x", encoding="utf-8") as stream:
        json.dump(dict(value, receipt_version=1, tool=VERSION), stream, indent=2)
        stream.write("\n")


def verify_runtime(root, identity):
    require(not Path(root).is_symlink(), "runtime root symlink is not supported")
    root = Path(root).resolve(strict=True)
    sha(identity["source_revision"], 40)
    files = identity["files"]
    require(isinstance(files, dict) and "duel6r" in files, "missing runtime inventory")
    actual = {}
    for path in root.rglob("*"):
        require(not path.is_symlink(), "runtime symlinks are not supported")
        if path.is_file():
            actual[path.relative_to(root).as_posix()] = digest(path)
    for name, value in files.items():
        relative(name)
        sha(value)
    require(actual == files, "runtime inventory mismatch (including added/missing files)")
    require(files["duel6r"] == sha(identity["application_sha256"]), "application identity mismatch")
    require((root / "duel6r").stat().st_mode & 0o111, "application is not executable")
    for folder in ("data", "levels", "profiles", "shaders", "sound", "textures"):
        require((root / folder).is_dir(), "missing flat runtime folder: " + folder)
    return root
