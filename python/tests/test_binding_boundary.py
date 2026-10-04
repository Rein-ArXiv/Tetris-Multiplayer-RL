"""Check the actual extension's copy/reference contracts with a selected build."""
import gc
import importlib
import os
from pathlib import Path
import sys

import numpy as np
import pytest


@pytest.fixture(scope="module")
def native():
    directory = os.environ.get("TETRIS_PY_MODULE_DIR")
    if not directory:
        pytest.skip("set TETRIS_PY_MODULE_DIR to the fresh extension build directory")
    folder = Path(directory).resolve()
    sys.path.insert(0, str(folder))
    try:
        module = importlib.import_module("tetris_py")
        assert Path(module.__file__).resolve().parent == folder
        yield module
    finally:
        sys.path.remove(str(folder))


def test_grid_is_owned_snapshot(native):
    game = native.SimGame(77)
    grid = game.grid()
    assert grid.shape == (game.ROWS, game.COLS)
    assert grid.dtype == np.int32
    original = game.state_hash()
    grid[0, 0] = 99
    assert game.grid()[0, 0] != 99
    assert game.state_hash() == original
    snapshot = game.grid()
    place = game.legal_placements()[0]
    game.apply_placement(place.col, place.rot)
    assert np.count_nonzero(snapshot) == 0
    assert np.count_nonzero(game.grid()) > 0
    del game
    gc.collect()
    assert snapshot.shape == grid.shape
    assert grid[0, 0] == 99


def test_live_block_is_readonly_but_tracks_native_mutation(native):
    game = native.SimGame(77)
    block = game.current_block()
    before = block.row_offset
    with pytest.raises(AttributeError):
        block.row_offset = 100
    game.move_block_down()
    assert block.row_offset == before + 1
    # reference_internal keeps the owner alive, not a frozen past value.
    del game
    gc.collect()
    assert block.row_offset == before + 1
    assert len(block.cell_positions()) > 0


def test_clone_owns_independent_complete_state(native):
    game = native.SimGame(77)
    for _ in range(3):
        place = game.legal_placements()[0]
        game.apply_placement(place.col, place.rot)
    branch = game.clone()
    assert branch is not game
    assert branch.state_hash() == game.state_hash()
    assert branch.rng_state() == game.rng_state()
    before = game.state_hash()
    place = branch.legal_placements()[-1]
    branch.apply_placement(place.col, place.rot)
    assert game.state_hash() == before
    assert branch.state_hash() != before
    game.apply_placement(place.col, place.rot)
    assert branch.state_hash() == game.state_hash()
    del game
    gc.collect()
    branch.tick()


def test_native_integer_conversion_rejects_out_of_range_seed(native):
    for value in [-1, 1 << 64]:
        with pytest.raises(TypeError):
            native.SimGame(value)
    assert isinstance(native.SimGame((1 << 64)-1).state_hash(), int)
