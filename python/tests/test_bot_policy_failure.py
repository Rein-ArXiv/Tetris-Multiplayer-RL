"""Actual ORT failures must not become a successful reward-opponent fallback."""
from pathlib import Path
import sqlite3
import subprocess
import pytest
from .test_meta_db_smoke import _find_meta_bin,_free_port,_wait_listen,_post


def make_policy(path,value):
    onnx=pytest.importorskip('onnx')
    import numpy as np
    from onnx import helper,TensorProto,numpy_helper
    from common import BOARD_ROWS,BOARD_COLS,NUM_PIECE_TYPES,NUM_PLACEMENTS
    board=helper.make_tensor_value_info('board',TensorProto.FLOAT,[1,1,BOARD_ROWS,BOARD_COLS])
    current=helper.make_tensor_value_info('current',TensorProto.FLOAT,[1,NUM_PIECE_TYPES])
    next_=helper.make_tensor_value_info('next',TensorProto.FLOAT,[1,NUM_PIECE_TYPES])
    logits=helper.make_tensor_value_info('policy_logits',TensorProto.FLOAT,[1,NUM_PLACEMENTS])
    score=helper.make_tensor_value_info('value',TensorProto.FLOAT,[1])
    graph=helper.make_graph([
        helper.make_node('Constant',[],['policy_logits'],value=numpy_helper.from_array(np.full((1,NUM_PLACEMENTS),value,dtype=np.float32))),
        helper.make_node('Constant',[],['value'],value=numpy_helper.from_array(np.zeros((1,),dtype=np.float32)))
    ],'failure-fixture',[board,current,next_],[logits,score])
    model=helper.make_model(graph,opset_imports=[helper.make_opsetid('',17)]);model.ir_version=9
    onnx.save(model,path)


@pytest.fixture
def server(tmp_path,request):
    binary=_find_meta_bin()
    if not binary:pytest.skip('set TETRIS_META_BIN to the freshly built ORT-enabled server')
    (tmp_path/'assets').mkdir();(tmp_path/'model').mkdir()
    make_policy(tmp_path/'model/good.onnx',0)
    make_policy(tmp_path/'model/bad.onnx',float('nan'))
    (tmp_path/'assets/opponents.cfg').write_text(
        'good|Good|model/good.onnx|||Test|1|0|1\n'
        'bad|Bad|model/bad.onnx|||Test|1|0|1\n'
        'missing|Missing|model/missing.onnx|||Test|1|0|1\n')
    condition=getattr(request,'param','valid')
    if condition=='invalid':
        with (tmp_path/'assets/opponents.cfg').open('a') as config:config.write('bad row\n')
    elif condition=='missing_catalog':(tmp_path/'assets/opponents.cfg').unlink()
    elif condition=='empty':(tmp_path/'assets/opponents.cfg').write_text('# no official profiles\n')
    port=_free_port();process=subprocess.Popen([str(binary.resolve()),'--db',str(tmp_path/'meta.db'),
        '--http',f'127.0.0.1:{port}','--relay-secret','test-secret','--bot-rewards'],cwd=tmp_path,
        stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    try:
        assert _wait_listen(port),process.poll()
        base=f'http://127.0.0.1:{port}';status,body=_post(base+'/v1/guest');assert status==200
        yield base,body['token'],tmp_path
    finally:process.terminate();process.communicate(timeout=10)


def test_missing_model_cannot_issue_reward_ticket(server):
    url,token,_=server
    status,body=_post(url+'/v1/bots/challenge',{'token':token,'opponent_id':'missing'})
    assert status==503 and body['error']=='model_unavailable'


def test_runtime_nonfinite_is_unavailable_without_wallet_write(server):
    url,token,root=server
    status,challenge=_post(url+'/v1/bots/challenge',{'token':token,'opponent_id':'bad'})
    assert status==200,challenge
    # Replacing this path must not replace the already prepared ticket policy.
    make_policy(root/'model/bad.onnx',0)
    payload={'token':token,'ticket':challenge['ticket'],'inputs_hex':'00'}
    status,body=_post(url+'/v1/bots/claim',payload)
    assert status==503 and body['error']=='policy_unavailable',body
    with sqlite3.connect(root/'meta.db') as db:
        assert db.execute('SELECT bp FROM players').fetchall()==[(0,)]
        assert db.execute('SELECT count(*) FROM bot_rewards').fetchone()==(0,)


def test_prepared_model_survives_path_removal(server):
    url,token,root=server
    (root/'model/good.onnx').unlink()
    status,challenge=_post(url+'/v1/bots/challenge',{'token':token,'opponent_id':'good'})
    assert status==200,challenge
    status,body=_post(url+'/v1/bots/claim',{'token':token,'ticket':challenge['ticket'],'inputs_hex':'00'})
    assert status==422 and body['error']=='victory_not_verified',body


@pytest.mark.parametrize('server',['invalid','missing_catalog','empty'],indirect=True)
def test_unusable_official_catalog_never_becomes_default_reward_partner(server):
    url,token,_=server
    status,body=_post(url+'/v1/bots/challenge',{'token':token,'opponent_id':'heuristic'})
    assert status==503 and body['error']=='catalog_unavailable',body


def test_automatic_model_path_is_not_a_reward_identity(server):
    url,token,_=server
    status,body=_post(url+'/v1/bots/challenge',{'token':token,'opponent_id':'model/good.onnx'})
    assert status==404 and body['error']=='unknown_opponent',body
