"""Portable filesystem regressions for the real macOS package staging helper."""

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location(
    "macos_package", Path(__file__).resolve().parents[1] / "macos/package.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class MacPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.source = root / "source"
        self.build = root / "build"
        self.output = root / "output"
        self.resources = self.source / "resources"
        self.resources.mkdir(parents=True)
        self.built = self.build / "Duel 6 Reloaded.app"
        (self.built / "Contents/Resources").mkdir(parents=True)
        (self.built / "Contents/MacOS").mkdir()
        (self.built / "Contents/MacOS/Duel 6 Reloaded").write_bytes(b"built executable")
        (self.built / "Contents/Info.plist").write_bytes(b"built plist")
        self.output.mkdir()

    def stage(self):
        return package.stage_app(self.source, self.build, self.output)

    def test_repeated_packaging_removes_deleted_resources_from_reused_build(self):
        (self.resources / "levels").mkdir()
        arena = self.resources / "levels/removed.json"
        arena.write_text("old arena")
        # CMake's incremental bundle still contains an asset deleted in source.
        stale = self.built / "Contents/Resources/levels/removed.json"
        stale.parent.mkdir()
        stale.write_text("old arena")
        app = self.stage()
        self.assertTrue((app / "Contents/Resources/levels/removed.json").exists())
        arena.unlink()
        (self.resources / "levels/current.json").write_text("current arena")
        self.stage()
        self.assertFalse((app / "Contents/Resources/levels/removed.json").exists())
        self.assertEqual("current arena", (app / "Contents/Resources/levels/current.json").read_text())
        self.assertTrue(stale.exists())  # Staging must not mutate build inputs.
        self.assertEqual(b"built executable", (app / "Contents/MacOS/Duel 6 Reloaded").read_bytes())
        self.assertEqual(b"built plist", (app / "Contents/Info.plist").read_bytes())

    def test_current_assets_replace_old_contents_and_person_data_is_not_packaged(self):
        (self.resources / "data").mkdir()
        config = self.resources / "data/config.cfg"
        config.write_text("current settings")
        save = self.resources / "data/persons.json"
        save.write_text("private developer data")
        (self.built / "Contents/Resources/stale-license.txt").write_text("old")
        app = self.stage()
        staged = app / "Contents/Resources"
        self.assertEqual("current settings", (staged / "data/config.cfg").read_text())
        self.assertFalse((staged / "stale-license.txt").exists())
        self.assertFalse((staged / "data/persons.json").exists())
        self.assertEqual("private developer data", save.read_text())
        (staged / "previous-package-metadata.json").write_text("old metadata")
        config.write_text("updated settings")
        self.stage()
        self.assertEqual("updated settings", (staged / "data/config.cfg").read_text())
        self.assertFalse((staged / "previous-package-metadata.json").exists())


if __name__ == "__main__":
    unittest.main()
