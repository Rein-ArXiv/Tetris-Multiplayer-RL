"""Schema/axis/copy contracts, hand-made asymmetric board, real Round snapshots."""
import argparse
import gc
from pathlib import Path
import sys
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"python"))
from observation import encode, observe, one_hot, stack_batch, to_tensors


def require(value, message):
    if not value:
        raise AssertionError(message)


def rejects(call):
    try:
        call()
    except ValueError:
        return
    raise AssertionError("invalid observation accepted")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-dir", required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.module_dir)
    import study_py as sim
    import torch
    # A deliberately non-square board and non-contiguous ids expose axis/id mistakes.
    raw = np.array([[0, 1, 0], [1, 0, 1]], dtype=np.int32)
    snapshot = {"board": raw, "current_id": 42, "next_id": 7}
    arrays = encode(snapshot, 2, 3, (7, 42))
    np.testing.assert_array_equal(arrays["board"], [[[0,1,0],[1,0,1]]])
    np.testing.assert_array_equal(arrays["current"], [0,1])
    np.testing.assert_array_equal(arrays["next"], [1,0])
    require(not np.shares_memory(raw, arrays["board"]), "source independent")
    for value in arrays.values():
        require(value.dtype == np.float32 and value.flags.c_contiguous, "dtype and layout")
    tensors = to_tensors(arrays)
    tensors["board"][0, 0, 0] = 1
    require(arrays["board"][0,0,0] == 1 and raw[0,0] == 0, "intentional intermediate sharing")
    saved = tensors["board"].clone()
    del arrays
    gc.collect()
    require(torch.equal(saved, tensors["board"]), "tensor owns array lifetime")
    unbatched = encode(snapshot, 2, 3, (7,42))
    batch = stack_batch([unbatched, unbatched])
    require(batch["board"].shape == (2,1,2,3), "NCHW")
    batch["board"][0,0,0,0] = 1
    require(unbatched["board"][0,0,0] == 0 and batch["board"][1,0,0,0] == 0, "stack allocation")
    rejects(lambda: stack_batch([]))
    rejects(lambda: stack_batch([unbatched, {}]))
    rejects(lambda: one_hot(-1,2))
    rejects(lambda: one_hot(2,2))
    for ids in [(), (7,7), (7,42.0), (True,42)]:
        rejects(lambda: encode(snapshot,2,3,ids))
    for pid in [-1, 99, 42.0, True]:
        rejects(lambda: encode(dict(snapshot,current_id=pid),2,3,(7,42)))
    for bad in [np.zeros((3,2)), np.full((2,3),np.nan), np.full((2,3),np.inf),
                np.full((2,3),1+1e-10), np.full((2,3),1e-50),
                np.full((2,3),"1"), np.full((2,3),1+0j), [[0],[1,0]], np.full((2,3),-1)]:
        rejects(lambda: encode(dict(snapshot,board=bad),2,3,(7,42)))

    schema = sim.observation_schema()
    a = sim.Session(77,2)
    b = a.clone()
    before = observe(a,schema)
    saved_state = a.state_bytes()
    b.step(0)
    require(a.state_bytes() != b.state_bytes(), "hidden state differs")
    require(all(np.array_equal(before[k],observe(b,schema)[k]) for k in before), "partial observation collision")
    # Actual locks/rotations preserve shape, vocabulary semantics and old arrays.
    seen = set()
    nonempty = terminal = False
    for seed in range(1,17):
        game = sim.Session(seed)
        for i in range(50):
            snapshot = game.snapshot()
            seen.add(snapshot["current_id"])
            obs = observe(game,schema)
            require(obs["board"].shape == (1,sim.ROWS,sim.COLS), "native dimensions")
            np.testing.assert_array_equal(obs["board"][0],np.asarray(game.grid()))
            require(obs["current"].argmax() == schema["piece_ids"].index(snapshot["current_id"]), "native id order")
            require(obs["next"].argmax() == schema["piece_ids"].index(snapshot["next_id"]), "preview order")
            nonempty |= bool(obs["board"].any())
            if game.finished():
                terminal = True
                break
            copied = {k:v.copy() for k,v in obs.items()}
            game.step(sim.DROP | (sim.ROTATE if i%2 else sim.LEFT))
            for k in obs: np.testing.assert_array_equal(obs[k],copied[k])
    require(seen == set(schema["piece_ids"]) and nonempty and terminal, "catalog/locked/terminal coverage")
    require(a.state_bytes() == saved_state, "observation is read-only")
    before["board"][0,0,0] = 1
    require(a.state_bytes() == saved_state, "observation mutation isolated")
    rejects(lambda: observe(a,dict(schema,version=2)))
    print("Observation contract: native states, asymmetric axes, non-contiguous ids, strict values passed")
    print("CPU tensor sharing/lifetime, independent snapshots/batches and partial-state example passed")


if __name__ == "__main__":
    main()
