import pytest
from .determinism_reference import compare_records, parse_reference
from .test_determinism_crossplatform import SEEDS, SCRIPT, REFERENCE_FILE


def parse(text):
    return parse_reference(text, SEEDS, SCRIPT)


def test_committed_capture_and_initial_final():
    references = parse(REFERENCE_FILE.read_text())
    assert list(references) == SEEDS
    assert all(len(r.records) == len(SCRIPT) for r in references.values())
    assert references[1].initial_hash == 0xfb7249998a4f8ed6
    assert references[1].final_hash == references[1].records[-1].state_hash


@pytest.mark.parametrize("text", ["", "\n", "not a trace", "==== seed 0x0000000000000001 ====\n"])
def test_empty_or_partial_is_an_error(text):
    with pytest.raises(ValueError): parse(text)


@pytest.mark.parametrize("change", [
    lambda s: s.split("==== seed ")[0],
    lambda s: s[:s.index("==== seed ", 10)],
    lambda s: s + s,
    lambda s: s.replace("step=001", "step=000", 1),
    lambda s: s.replace("mask=0x01", "mask=0x02", 1),
    lambda s: s.replace("total_ticks=30", "total_ticks=31", 1),
    lambda s: s.replace("score=0", "score=2147483648", 1),
    lambda s: s.replace("final_score=0", "final_score=1", 1),
    lambda s: "\n".join(line for line in s.splitlines() if not line.startswith("step=030")),
    lambda s: s + "unexpected tail\n",
])
def test_structural_corruption(change):
    with pytest.raises(ValueError): parse(change(REFERENCE_FILE.read_text()))


def test_early_terminal_requires_marker_and_summary():
    text = "\n".join([
        "==== seed 0x0000000000000001 ====", "seed=0x0000000000000001",
        "initial_hash=0x0000000000000000",
        "step=000 mask=0x00 ticks=30 total_ticks=30 score=7 over=1 hash=0x0000000000000001",
        "game_over_at_step=1", "final_hash=0x0000000000000001 final_score=7 final_over=1",
    ])
    assert len(parse_reference(text, [1], SCRIPT)[1].records) == 1
    with pytest.raises(ValueError): parse_reference(text.replace("game_over_at_step=1", "game_over_at_step=2"), [1], SCRIPT)
    with pytest.raises(ValueError): parse_reference(text.replace("game_over_at_step=1", ""), [1], SCRIPT)


def test_first_difference_and_equal_prefix_lengths():
    rows = [(0, 30, 0, False, 1), (1, 31, 0, False, 2)]
    assert compare_records(rows, rows) is None
    assert "record count" in compare_records(rows, rows[:1])
    assert "record count" in compare_records(rows[:1], rows)
    assert "state_hash" in compare_records(rows, [rows[0], (1, 31, 0, False, 3)])
    assert "score" in compare_records(rows, [(0, 30, 1, False, 1), rows[1]])
    assert "invalid field count" in compare_records(rows, [(0,)])
