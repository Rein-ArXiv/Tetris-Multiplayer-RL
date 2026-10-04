from pathlib import Path
import importlib.util
import zipfile
import pytest

spec = importlib.util.spec_from_file_location("package_opponents", Path(__file__).resolve().parents[1] / "tools/package_opponents.py")
bundler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundler)


def test_bundle_excludes_checkpoints_and_unreferenced_files(tmp_path):
    (tmp_path / "assets").mkdir()
    (tmp_path / "model").mkdir()
    (tmp_path / "model/bot.onnx").write_bytes(b"test fixture, not a model")
    (tmp_path / "model/private.pt").write_bytes(b"checkpoint")
    (tmp_path / "assets/opponents.cfg").write_text("a|A|model/bot.onnx|||Easy|6|18|60\nb|B|model/bot.onnx|||Normal|4|12|45\n")
    out = tmp_path / "bundle.zip"
    bundler.package(tmp_path, out)
    with zipfile.ZipFile(out) as archive:
        assert set(archive.namelist()) == {"assets/opponents.cfg", "model/bot.onnx", "opponents-manifest.json"}
    with pytest.raises(FileExistsError):
        bundler.package(tmp_path, out)


@pytest.mark.parametrize("model", ["../private.onnx", "/model/a.onnx", "model/../../private.onnx", "model/missing.onnx", "model/secret.pt"])
def test_bundle_rejects_missing_or_escaping_asset(tmp_path, model):
    (tmp_path / "assets").mkdir()
    (tmp_path / "assets/opponents.cfg").write_text(f"a|A|{model}|||Normal|6|18|60\n")
    with pytest.raises(ValueError):
        bundler.package(tmp_path, tmp_path / "bundle.zip")


def config_only(root):
    (root/'assets').mkdir()
    config=root/'assets/opponents.cfg'
    config.write_text('a|A|@heuristic|||Practice|6|18|60\n',encoding='utf-8')
    return config


def test_config_outside_root_is_rejected(tmp_path):
    root=tmp_path/'root';root.mkdir();(root/'assets').mkdir()
    source=tmp_path/'outside.cfg';source.write_text('a|A|@heuristic|||N|1|0|1\n')
    try:(root/'assets/opponents.cfg').symlink_to(source)
    except OSError:pytest.skip('symlink privileges unavailable')
    with pytest.raises(ValueError,match='external'):bundler.package(root,root/'bundle.zip')
    assert not (root/'bundle.zip').exists()


def test_captured_config_is_the_archived_config(tmp_path,monkeypatch):
    config=config_only(tmp_path);original=config.read_bytes()
    real=bundler.publish_archive
    def changed_source(out,captured,members):
        config.write_text('b|Changed|model/missing.onnx|||N|1|0|1\n')
        real(out,captured,members)
    monkeypatch.setattr(bundler,'publish_archive',changed_source)
    out=tmp_path/'bundle.zip';bundler.package(tmp_path,out)
    import hashlib,json
    with zipfile.ZipFile(out) as archive:
        assert archive.read('assets/opponents.cfg')==original
        manifest=json.loads(archive.read('opponents-manifest.json'))
        assert manifest['sha256']['assets/opponents.cfg']==hashlib.sha256(original).hexdigest()


def test_zip_write_failure_does_not_publish(tmp_path,monkeypatch):
    config_only(tmp_path);out=tmp_path/'bundle.zip'
    def fail(*args,**kwargs):raise OSError('injected write failure')
    monkeypatch.setattr(zipfile.ZipFile,'writestr',fail)
    with pytest.raises(OSError,match='injected'):bundler.package(tmp_path,out)
    assert not out.exists() and not list(tmp_path.glob('.opponents-*.tmp'))


def test_publication_race_preserves_winner(tmp_path,monkeypatch):
    config_only(tmp_path);out=tmp_path/'bundle.zip';real=bundler.os.link
    def race(src,dst):out.write_bytes(b'other publisher');real(src,dst)
    monkeypatch.setattr(bundler.os,'link',race)
    with pytest.raises(FileExistsError):bundler.package(tmp_path,out)
    assert out.read_bytes()==b'other publisher'
    assert not list(tmp_path.glob('.opponents-*.tmp'))


def test_dangling_output_link_is_preserved(tmp_path):
    config_only(tmp_path);out=tmp_path/'bundle.zip';elsewhere=tmp_path/'absent.zip'
    try:out.symlink_to(elsewhere)
    except OSError:pytest.skip('symlink privileges unavailable')
    with pytest.raises(FileExistsError):bundler.package(tmp_path,out)
    assert out.is_symlink() and not elsewhere.exists()


def test_identical_inputs_have_identical_archives(tmp_path):
    config_only(tmp_path);first=tmp_path/'first.zip';second=tmp_path/'second.zip'
    bundler.package(tmp_path,first);bundler.package(tmp_path,second)
    assert first.read_bytes()==second.read_bytes()


def test_dot_asset_is_rejected_without_index_error(tmp_path):
    config=config_only(tmp_path);config.write_text('a|A|.|||N|1|0|1\n')
    with pytest.raises(ValueError):bundler.package(tmp_path,tmp_path/'bundle.zip')
