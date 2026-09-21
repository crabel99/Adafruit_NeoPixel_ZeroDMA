import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DmaLifecycleContractTest(unittest.TestCase):
    def test_dma_descriptor_is_finite_and_completion_driven(self):
        source = (ROOT / "Adafruit_NeoPixel_ZeroDMA.cpp").read_text()
        header = (ROOT / "Adafruit_NeoPixel_ZeroDMA.h").read_text()

        self.assertNotIn("dma.loop(true)", source)
        self.assertIn("dma.setCallback(dmaCallback)", source)
        self.assertIn("static void dmaCallback(Adafruit_ZeroDMA *dma)", header)
        self.assertIn("uint8_t *stagingBuf", header)
        self.assertIn("volatile bool dmaActive", header)
        self.assertIn("volatile bool refreshPending", header)

    def test_show_starts_jobs_instead_of_begin(self):
        source = (ROOT / "Adafruit_NeoPixel_ZeroDMA.cpp").read_text()
        begin_body = source.split(
            "bool Adafruit_NeoPixel_ZeroDMA::begin(void)", maxsplit=1
        )[1].split("void Adafruit_NeoPixel_ZeroDMA::encodeInto", maxsplit=1)[0]
        show_body = source.split("void Adafruit_NeoPixel_ZeroDMA::show(void)", maxsplit=1)[1]
        self.assertNotIn("dma.startJob()", begin_body)
        self.assertIn("startTransfer(dmaBuf)", show_body)


if __name__ == "__main__":
    unittest.main()
