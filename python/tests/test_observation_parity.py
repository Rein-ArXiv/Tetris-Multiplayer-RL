"""Compare actual Python/PyTorch features with bot::observe for the same SimGame."""
import importlib
import json
import os
from pathlib import Path
import subprocess
import sys

import numpy as np
import pytest

from common import BOARD_ROWS, BOARD_COLS, NUM_PIECE_TYPES
from common.obs import build_observation, _piece_one_hot


@pytest.fixture(scope="module")
def actual():
    module_dir = os.environ.get("TETRIS_PY_MODULE_DIR")
    oracle = os.environ.get("TETRIS_OBSERVATION_DUMP")
    if not module_dir or not oracle:
        pytest.skip("set the fresh native extension directory and observation_dump executable")
    pytest.importorskip("torch")
    directory = str(Path(module_dir).resolve())
    sys.path.insert(0,directory)
    try:
        native = importlib.import_module("tetris_py")
        assert Path(native.__file__).resolve().parent == Path(directory)
        yield native,oracle
    finally:
        sys.path.remove(directory)


def test_actual_cpp_python_observation_stream(actual):
    import torch
    native,oracle = actual
    commands = (["0","1","8","0","2","4","16","g3","0","16"]*20)
    seen = set()
    occupied = ended = False
    for seed in range(1,17):
        expected = [json.loads(line) for line in subprocess.run(
            [oracle,str(seed),*commands],check=True,capture_output=True,text=True,timeout=15).stdout.splitlines()]
        assert len(expected) == len(commands)+1
        game = native.SimGame(seed)
        for index,record in enumerate(expected):
            if index:
                command = commands[index-1]
                if command.startswith("g"):
                    game.add_pending_garbage(int(command[1:]))
                else:
                    game.submit_input(int(command)); game.tick()
            assert record["hash"] == game.state_hash()
            assert (record["rows"],record["cols"],record["kinds"]) == (BOARD_ROWS,BOARD_COLS,NUM_PIECE_TYPES)
            obs = build_observation(game)
            shapes = {"board":(1,BOARD_ROWS,BOARD_COLS),"current":(NUM_PIECE_TYPES,),"next":(NUM_PIECE_TYPES,)}
            for name,tensor in obs.items():
                assert tensor.dtype == torch.float32 and tensor.device.type == "cpu"
                assert tensor.is_contiguous() and tuple(tensor.shape) == shapes[name]
                np.testing.assert_array_equal(tensor.numpy().reshape(-1),record[name])
            seen.add(record["current_id"])
            occupied |= bool(obs["board"].any())
            ended |= bool(record["ended"])
    assert seen == set(range(1,NUM_PIECE_TYPES+1)) and occupied and ended


def test_observation_read_does_not_mutate_or_alias_sim(actual):
    import torch
    native,_ = actual
    game = native.SimGame(77)
    before = game.state_hash()
    first = build_observation(game)
    second = build_observation(game)
    first["board"][0,0,0] = 1
    assert second["board"][0,0,0] == 0
    assert game.grid()[0,0] == 0 and game.state_hash() == before
    snapshot = {k:v.clone() for k,v in second.items()}
    game.submit_input(16);game.tick()
    for k in second:
        assert torch.equal(second[k],snapshot[k])


def test_legacy_unknown_piece_and_ghost_encoding(actual):
    # Explicit current schema edge cases; native live boards do not contain ghosts.
    for pid in [0,-1,NUM_PIECE_TYPES+1]:
        assert np.count_nonzero(_piece_one_hot(pid)) == 0
    class Fixture:
        def grid(self):
            board = np.zeros((BOARD_ROWS,BOARD_COLS),dtype=np.int32)
            board[0,:3] = [1,8,-1]
            return board
        def current_block_id(self):return 1
        def next_block_id(self):return NUM_PIECE_TYPES
    np.testing.assert_array_equal(build_observation(Fixture())["board"][0,0,:3],[1,0,0])


def test_hidden_garbage_changes_state_without_changing_features(actual):
    import torch
    native,_ = actual
    a = native.SimGame(77)
    b = a.clone();b.add_pending_garbage(3)
    assert a.state_hash() != b.state_hash()
    left,right = build_observation(a),build_observation(b)
    assert all(torch.equal(left[k],right[k]) for k in left)
    placement = a.legal_placements()[0]
    a.apply_placement(placement.col,placement.rot)
    b.apply_placement(placement.col,placement.rot)
    assert not torch.equal(build_observation(a)["board"],build_observation(b)["board"])



def test_board_shape_mismatch_is_rejected_before_model(actual):
    class Fixture:
        def grid(self):return np.zeros((BOARD_COLS,BOARD_ROWS),dtype=np.int32)
        def current_block_id(self):return 1
        def next_block_id(self):return 1
    with pytest.raises(ValueError,match="board shape"):
        build_observation(Fixture())
