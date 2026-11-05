#!/usr/bin/env python3
"""Offline schema/static checks, not a substitute for actual Chromium assertions."""
import json
from pathlib import Path
import tempfile
import unittest

from NativeVisualViewerFixture import prepare


class ViewerTests(unittest.TestCase):
    def test_fixtures_preserve_bad_pts_case_and_expose_real_nonqualifying_gap(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/"fixtures"; prepare(root)
            good=json.loads((root/"good/index.json").read_text())
            broken=json.loads((root/"gap/index.json").read_text())
            partial=json.loads((root/"inspection-gap/index.json").read_text())
            unknown=json.loads((root/"inspection-unknown/index.json").read_text())
            self.assertTrue(good["valid"])
            self.assertEqual(broken["frames"][0]["pts"],broken["frames"][1]["pts"])
            self.assertNotEqual(broken["validation"]["source_pts"][1],broken["frames"][1]["pts"])
            self.assertFalse(partial["valid"])
            self.assertTrue(partial["inspection_available"])
            self.assertTrue(partial["inspection_only"])
            self.assertIsInstance(partial["frames"][-1]["duration"],int)
            self.assertGreater(partial["frames"][-1]["duration"],0)
            self.assertIsNotNone(partial["validation"]["content_end"])
            self.assertEqual("known",partial["qualification"]["boundary"]["status"])
            self.assertFalse(partial["qualification"]["boundary"]["valid"])
            self.assertFalse(partial["qualification"]["count"]["valid"])
            self.assertEqual(6,partial["qualification"]["count"]["decoded_frames"])
            self.assertEqual(9,partial["qualification"]["count"]["expected_frames"])
            self.assertEqual(112367,partial["frames"][4]["gap_before_ticks"])
            self.assertEqual(1,partial["qualification"]["cadence"]["gaps_over_limit"])
            self.assertFalse(unknown["valid"])
            self.assertTrue(unknown["inspection_available"])
            self.assertTrue(unknown["inspection_only"])
            self.assertIsNone(unknown["frames"][-1]["duration"])
            self.assertIsNone(unknown["validation"]["content_end"])
            self.assertEqual("unknown",unknown["qualification"]["boundary"]["status"])
            self.assertIsNone(unknown["qualification"]["boundary"]["valid"])
            self.assertEqual(partial["validation"]["source_pts"],unknown["validation"]["source_pts"])
            self.assertEqual(partial["qualification"]["count"],unknown["qualification"]["count"])
            self.assertEqual(partial["qualification"]["cadence"],unknown["qualification"]["cadence"])
            self.assertIn("unsupported multi-track timing fixture",unknown["original"]["provenance"])

    def test_viewer_keeps_qualification_visible_and_checks_lifetimes_gaps_and_hashes(self):
        text=(Path(__file__).resolve().parents[1]/"tools/native_visual/viewer.html").read_text()
        for required in ('id="qualification" role="note"', 'NONQUALIFYING — inspection only',
                         'UNKNOWN (not inferred)', 'Original qualification rejection:',
                         'item.pts !== value.validation.source_pts[i]', 'gap<=0',
                         'item.gap_before_ticks !== gap', '!value.inspection_only || value.valid',
                         'annotatedGaps !== qualification.cadence.gaps_over_limit',
                         'item.duration !== (duration !== null && duration>0 ? duration : null)',
                         'crypto.subtle.digest("SHA-256", bytes)', 'hash !== item.sha256',
                         'await image.decode()', 'image.naturalWidth !== data.validation.width',
                         'if (await show(ordinal+1)) advance()'):
            with self.subTest(required=required): self.assertIn(required,text)
        # Dynamic loading/play/pause statuses must not overwrite the permanent warning.
        self.assertNotIn('$("qualification").textContent = text',text)


if __name__=="__main__": unittest.main()
