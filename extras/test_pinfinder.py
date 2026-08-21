import unittest

import pinfinder


class DeviceMacroGenerationTests(unittest.TestCase):
    def test_m4_devices_use_exact_cmsis_chip_guards(self):
        expected = {
            "SAMD51J19": ["__SAMD51J19A__"],
            "SAME51J19": ["__SAME51J19A__"],
            "SAME53N20": ["__SAME53N20A__"],
            "SAME54P20": ["__SAME54P20A__"],
        }
        for device, guards in expected.items():
            with self.subTest(device=device):
                self.assertEqual(guards, pinfinder.macro_candidates(device))

    def test_same54_package_tiers_remain_distinct(self):
        generated = pinfinder.generate_include()
        tier = generated.split("#ifndef SAMD5X_E5X_120_SERIES", 1)[1].split(
            "#endif", 1
        )[0]
        self.assertIn("__SAME54P20A__", tier)
        self.assertNotIn("__SAME54N20A__", tier)

    def test_samd21_existing_revision_suffix_is_preserved(self):
        self.assertEqual(["__SAMD21G18A__"], pinfinder.macro_candidates("SAMD21G18A"))


if __name__ == "__main__":
    unittest.main()
