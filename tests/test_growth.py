import unittest

from sapling_pet.growth import DAILY_POINT_CAP, earned_points, stage_for, stage_index_for


class GrowthTests(unittest.TestCase):
    def test_points_are_awarded_each_25_minutes(self):
        self.assertEqual(earned_points(24 * 60 + 59), 0)
        self.assertEqual(earned_points(25 * 60), 1)
        self.assertEqual(earned_points(75 * 60), 3)

    def test_daily_points_are_capped(self):
        self.assertEqual(earned_points(24 * 60 * 60), DAILY_POINT_CAP)

    def test_stage_thresholds(self):
        self.assertEqual(stage_for(0)[0], "破土嫩芽")
        self.assertEqual(stage_for(10)[0], "小树苗")
        self.assertEqual(stage_for(25)[0], "青年树")
        self.assertEqual(stage_for(50)[0], "成熟树")
        self.assertEqual(stage_index_for(49), 2)
        self.assertEqual(stage_index_for(50), 3)


if __name__ == "__main__":
    unittest.main()
