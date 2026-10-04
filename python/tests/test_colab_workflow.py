"""Notebook process boundaries and experiment admission; no remote Colab required."""
import ast
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch
import pytest
from train import colab_runtime as runtime
from train.process_runner import run_logged
from train.training_commands import command_for

ROOT=Path(__file__).resolve().parents[2]


def test_notebook_code_is_plain_checked_python():
    for file in ('setup_colab.ipynb','train_model_zoo_colab.ipynb'):
        doc=json.loads((ROOT/'python/train'/file).read_text())
        code='\n'.join(''.join(c['source']) for c in doc['cells'] if c['cell_type']=='code')
        ast.parse(code)
        assert '!pip' not in code and '!rm' not in code and 'git\', \'pull' not in code
        assert 'sys.executable' in code and 'runtime.prepare_native' in code and 'runtime.run_smoke' in code
        assert all(not c.get('outputs') for c in doc['cells'])


@pytest.mark.parametrize('algo',['ppo','ppo_sparse','dqn','ddqn','cbmpi','cbmpi_value','reinforce','a2c','nstep_ac','cem','muzero'])
def test_presets_are_valid_argv(algo,tmp_path):
    for preset in ('smoke','long'):
        command=command_for(algo,'example',preset,tmp_path/'with spaces')
        assert command[:2]==[sys.executable,'-m']
        assert command[command.index('--out')+1]==str(tmp_path/'with spaces/example.pt')
    with pytest.raises(ValueError):command_for(algo,'example','smkoe',tmp_path)
    with pytest.raises(ValueError):command_for(algo,'../outside','smoke',tmp_path)


def test_real_child_error_and_log_preservation(tmp_path):
    log=tmp_path/'child.log'
    with pytest.raises(subprocess.CalledProcessError) as failure:
        run_logged([sys.executable,'-c',"print('evidence',flush=True);raise SystemExit(7)"],cwd=tmp_path,log_path=log)
    assert failure.value.returncode==7 and log.read_text()=='evidence\n'
    with pytest.raises(FileExistsError):
        run_logged([sys.executable,'-c',"print('replace')"],cwd=tmp_path,log_path=log)
    assert log.read_text()=='evidence\n'


def test_final_wait_interruption_terminates_child(tmp_path):
    class Fake:
        stdout=iter(())
        def __init__(self):
            self.stdout=type('Stream',(),{'__iter__':lambda _:iter(()),'close':lambda _:None})()
            self.terminated=False;self.waits=0
        def wait(self,timeout=None):
            self.waits+=1
            if self.waits==1:raise KeyboardInterrupt()
            return 0
        def poll(self):return None
        def terminate(self):self.terminated=True
    child=Fake()
    with patch('train.process_runner.subprocess.Popen',return_value=child),pytest.raises(KeyboardInterrupt):
        run_logged(['child'],cwd=tmp_path,log_path=tmp_path/'wait.log')
    assert child.terminated


def receipt(tmp_path):
    repo=tmp_path/'repo';(repo/'python').mkdir(parents=True)
    module=tmp_path/'native'/'tetris_py.fake';module.parent.mkdir();module.write_bytes(b'module')
    for path in ['CMakeLists.txt','python/requirements.txt','python/requirements-colab.txt']:
        (repo/path).write_text('fixture')
    return dict(repo=str(repo),module=str(module),module_dir=str(module.parent),
                module_sha256=runtime.digest(module),source_sha256=runtime.source_digest(repo),
                python=sys.executable,python_version=runtime.platform.python_version(),packages=runtime.package_versions(),
                smoke={'module_sha256':runtime.digest(module)})


def test_stale_receipt_rejected(tmp_path):
    ready=receipt(tmp_path);runtime.verify_preparation(ready)
    Path(ready['repo'],'CMakeLists.txt').write_text('changed')
    with pytest.raises(RuntimeError,match='stale'):runtime.verify_preparation(ready)


def test_failed_and_repeated_runs_are_not_success(tmp_path):
    ready=receipt(tmp_path);outputs=tmp_path/'runs'
    with patch.object(runtime,'launch_argv',return_value=[sys.executable,'-c',"raise SystemExit(9)"]):
        with pytest.raises(subprocess.CalledProcessError):
            runtime.run_experiment(ready,algo='ppo',run_name='broken',preset='smoke',checkpoint_dir=outputs)
    assert json.loads((outputs/'broken/result.json').read_text())['status']=='failed'
    with pytest.raises(FileExistsError):
        runtime.run_experiment(ready,algo='ppo',run_name='broken',preset='smoke',checkpoint_dir=outputs)
    with pytest.raises(RuntimeError,match='smoke'):
        runtime.run_experiment(ready,algo='ppo',run_name='long',preset='long',checkpoint_dir=outputs,smoke_run=outputs/'broken')
    assert not (outputs/'long').exists()


def test_success_smoke_and_long_gate(tmp_path):
    ready=receipt(tmp_path);outputs=tmp_path/'runs'
    def execute(command,*,cwd,log_path):
        # Test fixture for artifact admission; actual native/PPO runs are separate.
        directory=Path(log_path).parent
        (directory/(directory.name+'.pt')).write_bytes(b'fixture artifact')
        Path(log_path).write_text('fixture exit 0')
    with patch.object(runtime,'run_logged',side_effect=execute):
        smoke=runtime.run_experiment(ready,algo='ppo',run_name='smoke',preset='smoke',checkpoint_dir=outputs)
        long=runtime.run_experiment(ready,algo='ppo',run_name='long',preset='long',checkpoint_dir=outputs,smoke_run=smoke)
    assert json.loads((long/'result.json').read_text())['status']=='success'
    with pytest.raises(RuntimeError,match='smoke'):
        runtime.run_experiment(ready,algo='a2c',run_name='wrong',preset='long',checkpoint_dir=outputs,smoke_run=smoke)


def test_child_success_without_checkpoint_is_failure(tmp_path):
    ready=receipt(tmp_path)
    with patch.object(runtime,'launch_argv',return_value=[sys.executable,'-c','pass']):
        with pytest.raises(RuntimeError,match='checkpoint'):
            runtime.run_experiment(ready,algo='ppo',run_name='empty',preset='smoke',checkpoint_dir=tmp_path/'runs')
    assert json.loads((tmp_path/'runs/empty/result.json').read_text())['status']=='failed'
