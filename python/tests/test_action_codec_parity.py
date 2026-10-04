"""Actual C++ and Python integer encodings and requested input sequences."""
import itertools
import os
import subprocess
import pytest
from common import NUM_COLS,NUM_ROTATIONS
from common.action_mask import encode_action,decode_action
from netbot.input_expander import expand_placement


def test_actual_cpp_codec_and_expansion():
    executable=os.environ.get('TETRIS_ACTION_CODEC_DUMP')
    if not executable:pytest.skip('set TETRIS_ACTION_CODEC_DUMP to the built native driver')
    values=list(itertools.product(range(NUM_COLS),range(NUM_ROTATIONS),range(NUM_COLS),range(NUM_ROTATIONS)))
    source=''.join(' '.join(map(str,v))+'\n' for v in values)
    lines=subprocess.run([executable],input=source,text=True,capture_output=True,check=True,timeout=20).stdout.splitlines()
    assert len(lines)==len(values)
    for (col,rot,target_col,target_rot),line in zip(values,lines):
        encoded,decoded_col,decoded_rot,*masks=map(int,line.split())
        assert encoded==encode_action(target_col,target_rot)
        assert (decoded_col,decoded_rot)==decode_action(encoded)==(target_col,target_rot)
        assert masks==expand_placement(col,rot,target_col,target_rot)
