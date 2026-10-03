"""Portable filesystem regressions for the real macOS package staging helper."""

import importlib.util
import hashlib
import io
import json
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import tarfile
import unittest
from unittest.mock import patch

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

    def source_fixture(self, members=None):
        keg = self.build / "Cellar/example/1.0"
        (keg / ".brew").mkdir(parents=True)
        recipe = b"test recipe fixture"
        (keg / ".brew/example.rb").write_bytes(recipe)
        cache = self.build / "source-cache"
        cache.mkdir()
        archive = cache / "example-1.0.tar.gz"
        with tarfile.open(archive, "w:gz") as contents:
            for name, data in (members or {
                "example-1.0/LICENSES/LGPL-2.1-or-later.txt": b"fixture license text\n",
                "example-1.0/library.c": b"fixture source and copyright notices\n",
            }).items():
                entry = tarfile.TarInfo(name)
                entry.size = len(data)
                contents.addfile(entry, io.BytesIO(data))
        packet = {
            "version": "1.0", "recipe_sha256": [hashlib.sha256(recipe).hexdigest()],
            "required_notices": ["example-1.0/LICENSES/LGPL-2.1-or-later.txt"],
            "sources": [{"url": "https://example.invalid/example-1.0.tar.gz",
                         "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()}],
        }
        return keg, cache, packet

    def test_source_packet_retains_complete_verified_source_and_verbatim_notices(self):
        keg, cache, packet = self.source_fixture()
        licenses = self.output / "licenses"
        with patch.object(package, "run", side_effect=AssertionError("unexpected download")):
            result = package.collect_notices({"example": keg}, licenses, cache, {"example": packet})
        self.assertTrue(result[0]["source_packet"])
        self.assertEqual((cache / "example-1.0.tar.gz").read_bytes(),
                         (licenses / "example/sources/example-1.0.tar.gz").read_bytes())
        self.assertEqual(b"fixture license text\n", (licenses / "example/upstream" /
                         packet["required_notices"][0]).read_bytes())
        self.assertEqual((keg / ".brew/example.rb").read_bytes(),
                         (licenses / "example/example.rb").read_bytes())
        provenance = json.loads((licenses / "example/source-provenance.json").read_text())
        self.assertEqual(packet["sources"], provenance["sources"])

    def test_corrupt_source_and_changed_recipe_fail_closed(self):
        keg, cache, packet = self.source_fixture()
        archive = cache / "example-1.0.tar.gz"
        original = archive.read_bytes()
        archive.write_bytes(b"corrupt download")
        with self.assertRaisesRegex(RuntimeError, "Source checksum mismatch"):
            package.collect_notices({"example": keg}, self.output / "licenses", cache, {"example": packet})
        archive.write_bytes(original)
        (keg / ".brew/example.rb").write_text("different build modifications")
        with self.assertRaisesRegex(RuntimeError, "Unreviewed source/recipe"):
            package.collect_notices({"example": keg}, self.output / "licenses", cache, {"example": packet})

    def test_source_packet_rejects_traversal(self):
        keg, cache, packet = self.source_fixture({"../COPYING": b"unsafe path"})
        with self.assertRaisesRegex(RuntimeError, "Unsafe source archive path"):
            package.collect_notices({"example": keg}, self.output / "licenses", cache, {"example": packet})
        self.assertFalse((self.output / "COPYING").exists())

    def test_source_packet_rejects_missing_required_license(self):
        keg, cache, packet = self.source_fixture({"example-1.0/library.c": b"source without notice"})
        with self.assertRaisesRegex(RuntimeError, "lacks required notice"):
            package.collect_notices({"example": keg}, self.output / "licenses", cache, {"example": packet})

    def test_spdx_license_directories_and_all_missing_dependencies_are_audited(self):
        keg = self.build / "Cellar/with-notices/1.0"
        notice = keg / "share/doc/component/LICENSES/MIT.txt"
        notice.parent.mkdir(parents=True)
        notice.write_bytes(b"fixture license bytes\n")
        good = package.collect_notices({"with-notices": keg}, self.output / "licenses",
                                       self.build / "cache", {})
        self.assertEqual([str(notice.relative_to(keg))], good[0]["installed_notices"])
        self.assertEqual(notice.read_bytes(), (self.output / "licenses/with-notices" /
                                              notice.relative_to(keg)).read_bytes())
        missing = {name: self.build / "Cellar" / name / "1.0" for name in ("missing-a", "missing-b")}
        for path in missing.values():
            path.mkdir(parents=True)
            (path / "AUTHORS").write_text("Attribution alone is not a license grant")
        with self.assertRaises(RuntimeError) as caught:
            package.collect_notices({**missing, "with-notices": keg}, self.output / "licenses",
                                    self.build / "cache", {})
        self.assertIn("missing-a@1.0", str(caught.exception))
        self.assertIn("missing-b@1.0", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
