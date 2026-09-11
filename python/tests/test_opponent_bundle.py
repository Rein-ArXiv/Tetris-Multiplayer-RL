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
