"""Tests for the profile line codec."""

import unittest

from tools.opponent_profile import encode_profile, split_profile_line

GOOD = [
    "player_1",
    "Jane Doe",
    "@heuristic",
    "icon.png",
    "portrait.png",
    "hard",
    "15",
    "30",
    "100",
]


class SplitProfileLineTests(unittest.TestCase):
    def test_round_trip(self):
        self.assertEqual(encode_profile(GOOD), "|".join(GOOD) + "\n")
        self.assertEqual(split_profile_line(encode_profile(GOOD)), GOOD)

    def test_bom_comment_and_outer_whitespace(self):
        line = (
            "\ufeff  player_1 | Jane Doe | @heuristic | icon.png | portrait.png | "
            "hard | 15 | 30 | 100  # trailing comment\r\n"
        )
        self.assertEqual(split_profile_line(line), GOOD)

    def test_unicode_display_name(self):
        fields = list(GOOD)
        fields[1] = "Jöhn Döe 😀"
        self.assertEqual(split_profile_line(encode_profile(fields)), fields)

    def test_display_name_byte_limit(self):
        fields = list(GOOD)
        fields[1] = "é" * 48  # exactly 96 UTF-8 bytes
        self.assertEqual(split_profile_line(encode_profile(fields)), fields)
        fields[1] = "é" * 49  # 98 UTF-8 bytes
        with self.assertRaises(ValueError):
            encode_profile(fields)
        with self.assertRaises(ValueError):
            split_profile_line("|".join(fields) + "\n")

    def test_blank_and_comment_lines_are_ignored(self):
        for line in ("", "   \t\r\n", "   # just a comment\n", "\ufeff# bom comment\n"):
            with self.subTest(line=line):
                self.assertIsNone(split_profile_line(line))

    def test_wrong_field_count_is_rejected(self):
        for line in ("a|b|c\n", "|".join(GOOD[:8]) + "\n", "|".join(GOOD) + "|x\n"):
            with self.subTest(line=line):
                with self.assertRaises(ValueError):
                    split_profile_line(line)

    def test_invalid_identity(self):
        for identity in ("Player_1", "", "with space", "a" * 33, "ábc"):
            fields = list(GOOD)
            fields[0] = identity
            with self.subTest(identity=identity):
                with self.assertRaises(ValueError):
                    split_profile_line("|".join(fields) + "\n")
                with self.assertRaises(ValueError):
                    encode_profile(fields)

    def test_invalid_ticks(self):
        cases = [
            (6, "0"),
            (6, "31"),
            (6, "1.5"),
            (6, "-1"),
            (6, ""),
            (7, "181"),
            (7, "x"),
            (8, "0"),
            (8, "601"),
        ]
        for index, value in cases:
            fields = list(GOOD)
            fields[index] = value
            with self.subTest(index=index, value=value):
                with self.assertRaises(ValueError):
                    split_profile_line("|".join(fields) + "\n")
                with self.assertRaises(ValueError):
                    encode_profile(fields)

    def test_required_fields(self):
        for index in (1, 2):
            fields = list(GOOD)
            fields[index] = ""
            with self.subTest(index=index):
                with self.assertRaises(ValueError):
                    split_profile_line("|".join(fields) + "\n")

    def test_non_string_line(self):
        with self.assertRaises(TypeError):
            split_profile_line(None)


class EncodeProfileTests(unittest.TestCase):
    def test_delimiter_and_comment_are_rejected(self):
        for bad in ("a|b", "a#b", "#leading", "trailing#"):
            fields = list(GOOD)
            fields[3] = bad
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    encode_profile(fields)

    def test_control_characters_are_rejected(self):
        for bad in ("a\x00b", "a\x1fb", "a\x7fb", "a\tb", "a\nb"):
            fields = list(GOOD)
            fields[4] = bad
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    encode_profile(fields)

    def test_outer_whitespace_is_rejected(self):
        for bad in (" name", "name ", "\tname", "name\n", "\vname"):
            fields = list(GOOD)
            fields[4] = bad
            with self.subTest(bad=bad):
                with self.assertRaises(ValueError):
                    encode_profile(fields)

    def test_non_string_field_is_rejected(self):
        fields = list(GOOD)
        fields[0] = 123
        with self.assertRaises(ValueError):
            encode_profile(fields)

    def test_difficulty_is_optional(self):
        fields = list(GOOD)
        fields[5] = ""
        self.assertEqual(split_profile_line(encode_profile(fields)), fields)

    def test_result_ends_with_newline(self):
        encoded = encode_profile(GOOD)
        self.assertTrue(encoded.endswith("\n"))
        self.assertEqual(encoded.count("\n"), 1)


if __name__ == "__main__":
    unittest.main()

# Primary review: cross-language behavior and actual release archive consumers.
import os
import json
import shlex
import subprocess
import zipfile
from pathlib import Path
import pytest
from tools.package_opponents import package


def test_actual_cpp_profile_parser():
    executable=os.environ.get('TETRIS_OPPONENT_PROFILE_DUMP')
    if not executable:pytest.skip('set TETRIS_OPPONENT_PROFILE_DUMP to the native parser driver')
    good=['r','별빛','@heuristic','','','Normal','6','18','60']
    records=['', '# comment','\ufeff'+ '|'.join(good)+' # note', '|'.join(good)]
    for column,values in [(0,['','UPPER','x'*33]),(1,['','별'*32,'별'*33,'line\u2028label']),
                          (2,['','model/shared.onnx']),(5,['','label']),
                          (6,['0','1','30','31','+1','01','9'*40]),
                          (7,['-0','0','180','181','2x']),(8,['0','1','600','601'])]:
        for value in values:
            fields=good.copy();fields[column]=value;records.append('|'.join(fields))
    records+=['|'.join(good)+'|extra','r|A\x00B|@heuristic|||N|6|18|60']
    output=subprocess.run([executable],input='\n'.join(records)+'\n',encoding='utf-8',capture_output=True,check=True,timeout=20).stdout.split('\n')[:-1]
    assert len(output)==len(records)
    for text,line in zip(records,output):
        try:fields=split_profile_line(text)
        except ValueError:assert line=='error';continue
        if fields is None:assert line=='skip';continue
        actual=shlex.split(line)
        assert actual==['ok',*fields[:5],fields[5] or 'Normal',*map(str,map(int,fields[6:]))]


def test_package_uses_runtime_comment_bom_semantics(tmp_path):
    (tmp_path/'assets').mkdir();cfg=tmp_path/'assets/opponents.cfg'
    cfg.write_text('\ufeffr|별\u2028빛|@heuristic|||Normal|6|18|60 # comment\n',encoding='utf-8')
    out=tmp_path/'pack.zip';package(tmp_path,out)
    with zipfile.ZipFile(out) as archive:
        assert archive.read('assets/opponents.cfg')==cfg.read_bytes()
        assert set(archive.namelist())=={'assets/opponents.cfg','opponents-manifest.json'}
    before=out.read_bytes()
    with pytest.raises(FileExistsError):package(tmp_path,out)
    assert out.read_bytes()==before


@pytest.mark.parametrize('asset',['../secret.onnx','/tmp/secret.onnx','model/../secret.onnx','model\\bad.onnx','model/missing.onnx'])
def test_package_path_boundary_stays_strict(tmp_path,asset):
    (tmp_path/'assets').mkdir()
    (tmp_path/'assets/opponents.cfg').write_text(encode_profile(['r','R',asset,'','','N','1','0','1']))
    with pytest.raises(ValueError):package(tmp_path,tmp_path/'bad.zip')
    assert not (tmp_path/'bad.zip').exists()


def test_notebook_registration_uses_validated_encoder():
    root=Path(__file__).resolve().parents[2]
    notebook=json.loads((root/'python/train/train_model_zoo_colab.ipynb').read_text())
    cell=next(''.join(c['source']) for c in notebook['cells'] if 'profile_line = encode_profile(fields)' in ''.join(c['source']))
    assert cell.index('encode_profile(fields)')<cell.index('cfg.write_text')
    assert 'encoding=\'utf-8\'' in cell
