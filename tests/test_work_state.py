import unittest

from sapling_pet.work_state import WorkMode, WorkStateTracker


class WorkStateTests(unittest.TestCase):
    def test_active_calm_and_sleeping(self):
        tracker = WorkStateTracker()
        self.assertEqual(tracker.update(idle=2, elapsed=1), WorkMode.ACTIVE)
        self.assertEqual(tracker.update(idle=90, elapsed=1), WorkMode.CALM)
        self.assertEqual(tracker.update(idle=901, elapsed=1), WorkMode.SLEEPING)

    def test_long_focus_becomes_fatigued(self):
        tracker = WorkStateTracker(fatigue_limit=10)
        self.assertEqual(tracker.update(idle=1, elapsed=6), WorkMode.ACTIVE)
        self.assertEqual(tracker.update(idle=1, elapsed=5), WorkMode.FATIGUED)

    def test_recent_intensity_is_calculated(self):
        tracker = WorkStateTracker(sample_window=4)
        tracker.update(idle=1, elapsed=2)
        tracker.update(idle=70, elapsed=2)
        self.assertEqual(tracker.intensity, 0.5)

    def test_pause_never_counts_as_active(self):
        tracker = WorkStateTracker()
        self.assertEqual(tracker.update(idle=0, elapsed=1, paused=True), WorkMode.CALM)
        self.assertEqual(tracker.intensity, 0.0)


if __name__ == "__main__":
    unittest.main()
