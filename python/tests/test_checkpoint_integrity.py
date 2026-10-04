"""Persistence boundaries: configuration, compatibility and interrupted replacement."""
from pathlib import Path
from unittest.mock import patch
import pytest
import torch
from common.models import TetrisPolicyNet
from common.checkpoint import save_checkpoint, load_checkpoint, CHECKPOINT_META_KEY


def small():
    return TetrisPolicyNet(conv_channels=(2, 3), hidden=8)


def test_custom_configuration_roundtrip(tmp_path):
    model = small()
    target = tmp_path/'model.pt'
    save_checkpoint(model, target)
    loaded = load_checkpoint(target)
    assert loaded.conv_channels == model.conv_channels and loaded.hidden == model.hidden
    assert not loaded.training
    for key, value in model.state_dict().items():
        assert torch.equal(value, loaded.state_dict()[key])


@pytest.mark.parametrize('reserved', ['arch_version', 'class', 'format_version', 'config', 'io_contract'])
def test_extra_cannot_override_contract(tmp_path, reserved):
    target = tmp_path/'model.pt'
    target.write_bytes(b'previous good file')
    with pytest.raises((ValueError, TypeError), match='reserved'):
        save_checkpoint(small(), target, extra={reserved: 'wrong'})
    assert target.read_bytes() == b'previous good file'


def test_serialization_failure_keeps_previous(tmp_path):
    target = tmp_path/'model.pt'
    target.write_bytes(b'previous good file')
    def fail(payload, stream):
        if hasattr(stream, 'write'): stream.write(b'partial')
        else: Path(stream).write_bytes(b'partial')
        raise OSError('write failed')
    with patch('torch.save', side_effect=fail), pytest.raises(OSError):
        save_checkpoint(small(), target)
    assert target.read_bytes() == b'previous good file'
    assert list(tmp_path.iterdir()) == [target]


def test_boolean_arch_version_rejected(tmp_path):
    target=tmp_path/'model.pt'
    torch.save({'state_dict': TetrisPolicyNet().state_dict(), CHECKPOINT_META_KEY:
                {'arch_version': True, 'class': 'TetrisPolicyNet'}}, target)
    with pytest.raises(RuntimeError, match='arch_version'):
        load_checkpoint(target)


@pytest.mark.parametrize('operation', ['fsync', 'replace'])
def test_commit_failure_keeps_previous(tmp_path, operation):
    target=tmp_path/'model.pt'; target.write_bytes(b'previous good file')
    with patch('common.atomic_save.os.'+operation, side_effect=OSError('injected')), pytest.raises(OSError):
        save_checkpoint(small(), target)
    assert target.read_bytes() == b'previous good file'
    assert list(tmp_path.iterdir()) == [target]


def test_legacy_default_still_loads(tmp_path):
    model=TetrisPolicyNet(); path=tmp_path/'old.pt'
    torch.save({'state_dict':model.state_dict(), '__meta__':
                {'arch_version':model.ARCH_VERSION,'class':'TetrisPolicyNet'}}, path)
    actual=load_checkpoint(path)
    assert all(torch.equal(v, actual.state_dict()[k]) for k,v in model.state_dict().items())


@pytest.mark.parametrize('case', ['format', 'meaning', 'shape', 'dtype', 'nan', 'missing_config', 'bad_config'])
def test_corrupt_contract_is_rejected(tmp_path, case):
    path=tmp_path/'bad.pt'; save_checkpoint(small(),path)
    payload=torch.load(path,weights_only=True)
    meta=payload['__meta__']; key=next(iter(payload['state_dict']))
    if case=='format': meta['format_version']=True
    elif case=='meaning': meta['io_contract']['observation_version']+=1
    elif case=='shape': payload['state_dict'][key]=payload['state_dict'][key][:1]
    elif case=='dtype': payload['state_dict'][key]=payload['state_dict'][key].double()
    elif case=='nan': payload['state_dict'][key].fill_(float('nan'))
    elif case=='missing_config': del meta['config']
    else: meta['config']['hidden']=True
    torch.save(payload,path)
    with pytest.raises(RuntimeError):load_checkpoint(path)


def test_invalid_save_does_not_replace(tmp_path):
    path=tmp_path/'model.pt'; path.write_bytes(b'previous')
    model=small()
    with torch.no_grad(): next(model.parameters()).fill_(float('nan'))
    with pytest.raises(RuntimeError):save_checkpoint(model,path)
    with pytest.raises(TypeError):save_checkpoint(small(),path,extra={'bad':object()})
    assert path.read_bytes()==b'previous'


def test_warm_start_missing_path_fails_before_environment(tmp_path):
    from train import ppo_tetris as ppo, policy_gradient_tetris as pg
    for module in (ppo,pg):
        args=module.build_argparser().parse_args(['--resume',str(tmp_path/'absent.pt')])
        with pytest.raises(FileNotFoundError):module.train(args)
