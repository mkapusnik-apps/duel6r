"""Portable filesystem regressions for the real macOS package staging helper."""

import importlib.util
from pathlib import Path
import plistlib
import subprocess
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

    def test_bundle_utilities_resolves_real_plist_template_and_staged_executable(self):
        # Exercise CMake's actual consumer, not a substitute XML parser: it
        # requires CFBundleExecutable's string on the line following the key.
        script = self.build / "check-bundle.cmake"
        script.write_text('''cmake_minimum_required(VERSION 3.16)
set(MACOSX_BUNDLE_EXECUTABLE_NAME "Duel 6 Reloaded")
configure_file("${TEMPLATE}" "${APP}/Contents/Info.plist")
include(BundleUtilities)
get_bundle_main_executable("${APP}" executable)
if(NOT executable STREQUAL "${APP}/Contents/MacOS/Duel 6 Reloaded")
    message(FATAL_ERROR "Bundle executable discovery failed: ${executable}")
endif()
get_bundle_and_executable("${APP}" bundle executable valid)
if(NOT valid)
    message(FATAL_ERROR "BundleUtilities rejected the generated bundle")
endif()
''')
        result = subprocess.run([
            "cmake", f"-DAPP={self.built}",
            f"-DTEMPLATE={Path(__file__).resolve().parents[1] / 'macos/Info.plist.in'}",
            "-P", str(script)], capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        app = self.stage()
        with (app / "Contents/Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
        self.assertEqual("Duel 6 Reloaded", info["CFBundleExecutable"])
        self.assertTrue((app / "Contents/MacOS" / info["CFBundleExecutable"]).is_file())
        self.assertEqual("14.0", info["LSMinimumSystemVersion"])

    def test_final_bundle_verification_rejects_a_missing_main_executable(self):
        root = Path(__file__).resolve().parents[1]
        info = (root / "macos/Info.plist.in").read_text().replace(
            "${MACOSX_BUNDLE_EXECUTABLE_NAME}", "Duel 6 Reloaded")
        (self.built / "Contents/Info.plist").write_text(info)
        (self.built / "Contents/MacOS/Duel 6 Reloaded").unlink()
        result = subprocess.run([
            "cmake", f"-DAPP={self.built}", "-P", str(root / "macos/VerifyBundle.cmake")],
            capture_output=True, text=True)
        self.assertNotEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("Invalid application bundle", result.stderr)

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
