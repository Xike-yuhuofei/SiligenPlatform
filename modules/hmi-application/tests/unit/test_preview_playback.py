import unittest

from bootstrap import ensure_hmi_application_test_paths

ensure_hmi_application_test_paths()

from hmi_application.domain.preview_session_types import PreviewSessionState
from hmi_application.preview_gate import DispensePreviewGate
from hmi_application.services.preview_playback import PreviewPlaybackController


class PreviewPlaybackControllerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.state = PreviewSessionState(gate=DispensePreviewGate())
        self.playback = PreviewPlaybackController(self.state)

    def test_load_builds_playback_model_from_points(self) -> None:
        self.playback.load(((0.0, 0.0), (3.0, 4.0), (6.0, 4.0)), estimated_time_s=2.5)

        status = self.playback.status()
        self.assertTrue(status.available)
        self.assertEqual(status.state, "idle")
        self.assertEqual(status.point_count, 3)
        self.assertEqual(status.duration_s, 2.5)

    def test_build_model_falls_back_to_default_speed_when_estimated_time_missing(self) -> None:
        model = PreviewPlaybackController.build_model(((0.0, 0.0), (20.0, 0.0)), estimated_time_s=0.0)

        self.assertEqual(model.total_length_mm, 20.0)
        self.assertEqual(model.duration_s, 1.0)

    def test_play_pause_resume_and_finish(self) -> None:
        self.playback.load(((0.0, 0.0), (0.0, 10.0)), estimated_time_s=1.0)

        self.playback.play(10.0)
        playing = self.playback.tick(10.25)
        paused = self.playback.pause(10.25)
        resumed = self.playback.play(10.25)
        finished = self.playback.tick(11.5)

        self.assertEqual(playing.state, "playing")
        self.assertGreater(playing.progress, 0.2)
        self.assertEqual(paused.state, "paused")
        self.assertGreater(paused.progress, 0.2)
        self.assertEqual(resumed.state, "playing")
        self.assertEqual(finished.state, "finished")
        self.assertEqual(finished.progress, 1.0)

    def test_speed_ratio_retimes_progress(self) -> None:
        self.playback.load(((0.0, 0.0), (0.0, 10.0)), estimated_time_s=1.0)

        self.playback.play(20.0)
        self.playback.set_speed_ratio(2.0, now_monotonic=20.0)
        status = self.playback.tick(20.25)

        self.assertEqual(status.speed_ratio, 2.0)
        self.assertGreater(status.progress, 0.45)

    def test_clear_resets_playback_state(self) -> None:
        self.playback.load(((0.0, 0.0), (0.0, 10.0)), estimated_time_s=1.0)
        self.playback.play(30.0)
        self.playback.tick(30.2)

        self.playback.clear()

        status = self.playback.status()
        self.assertFalse(status.available)
        self.assertEqual(status.state, "idle")
        self.assertEqual(status.progress, 0.0)
        self.assertEqual(status.duration_s, 0.0)


if __name__ == "__main__":
    unittest.main()
