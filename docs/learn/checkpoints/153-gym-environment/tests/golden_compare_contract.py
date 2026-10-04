"""Exercise comparison failures independently of the C++ game emitter."""
from pathlib import Path
import copy
import importlib.util
import json
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_golden", ROOT / "tools/check_golden.py")
golden = importlib.util.module_from_spec(spec)
spec.loader.exec_module(golden)

class Contract(unittest.TestCase):
    def setUp(self):
        self.trace = golden.parse_trace((ROOT / "golden/mixed-v1.json").read_bytes())

    def test_equal_and_metadata(self):
        self.assertIsNone(golden.compare_traces(self.trace, copy.deepcopy(self.trace)))
        for key in golden.METADATA:
            actual = copy.deepcopy(self.trace); actual[key] += "-changed"
            self.assertIn("metadata " + key, golden.compare_traces(self.trace, actual))

    def test_rows_and_lengths(self):
        for key in ("tick", "mask", "garbage", "step", "hash"):
            actual = copy.deepcopy(self.trace)
            actual["records"][1][key] = "changed" if key in {"step", "hash"} else 7
            self.assertIn(key, golden.compare_traces(self.trace, actual))
        for amount in (-1, 1):
            actual = copy.deepcopy(self.trace)
            if amount < 0: actual["records"].pop()
            else: actual["records"].append(copy.deepcopy(actual["records"][-1]))
            self.assertIn("records length", golden.compare_traces(self.trace, actual))

    def test_bytes_difference_and_prefix(self):
        actual = copy.deepcopy(self.trace)
        row = actual["records"][1]; data = bytearray.fromhex(row["bytes"]); data[10] ^= 1
        row["bytes"] = data.hex(); row["hash"] = golden.fnv64(data)
        self.assertIn("offset=10", golden.compare_traces(self.trace, actual))
        row["bytes"] = self.trace["records"][1]["bytes"] + "00"
        self.assertIn("bytes length", golden.compare_traces(self.trace, actual))

    def test_malformed(self):
        for data in (b"", b"{}", b"[]", b'{"format":1,"format":2}', b"null", b"\xff"):
            with self.assertRaises(ValueError): golden.parse_trace(data)
        for mutate in (
            lambda x: x.update(records=[]),
            lambda x: x.update(seed="-1"),
            lambda x: x.update(seed="18446744073709551616"),
            lambda x: x.update(scenario="unknown"),
            lambda x: x["records"][1].update(tick=3),
            lambda x: x["records"][1].update(mask=True),
            lambda x: x["records"][1].update(mask=256),
            lambda x: x["records"][1].update(garbage=21),
            lambda x: x["records"][1].update(step=[]),
            lambda x: x["records"][1].update(bytes="00"),
            lambda x: x["records"][1].update(hash="0000000000000000"),
            lambda x: x["records"][1].update(extra=1),
        ):
            actual = copy.deepcopy(self.trace); mutate(actual)
            with self.assertRaises(ValueError): golden.validate_trace(actual)

    def test_terminal_records(self):
        trace = golden.parse_trace((ROOT / "golden/overflow-v1.json").read_bytes())
        self.assertEqual([r["step"] for r in trace["records"]], ["initial", "game_over", "stopped", "stopped"])
        self.assertEqual(len({r["bytes"] for r in trace["records"][1:]}), 1)

if __name__ == "__main__": unittest.main()
