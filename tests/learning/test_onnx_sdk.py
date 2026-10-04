"""Execute the SDK preparation script with local archives, never a network fetch."""
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT/'third_party/fetch_onnxruntime.sh'


class SdkPreparation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sdk preparation ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        vendor = self.root/'third_party';vendor.mkdir()
        self.script = vendor/'fetch_onnxruntime.sh';shutil.copyfile(SCRIPT,self.script)
        self.sdk = vendor/'onnxruntime'
        self.package = self.root/'onnxruntime-linux-x64-1.18.1'
        (self.package/'include').mkdir(parents=True);(self.package/'lib').mkdir()
        for name in ['onnxruntime_c_api.h','onnxruntime_cxx_api.h','onnxruntime_cxx_inline.h','onnxruntime_float16.h']:
            (self.package/'include'/name).write_text('fixture header')
        (self.package/'lib/libonnxruntime.so.1.18.1').write_bytes(b'fixture library')
        (self.package/'lib/libonnxruntime.so').symlink_to('libonnxruntime.so.1.18.1')
        (self.package/'LICENSE').write_text('fixture upstream notice')
        self.archive = self.root/'input.tgz'
        tools = self.root/'tools';tools.mkdir()
        fake_uname = tools/'uname'
        fake_uname.write_text('#!/bin/sh\nif [ "$1" = -s ]; then echo Linux; else echo "${STUDY_CPU:-x86_64}"; fi\n')
        fake_curl = tools/'curl'
        fake_curl.write_text('#!/bin/sh\nwhile [ "$1" != -o ]; do shift; done\ncp "$STUDY_ARCHIVE" "$2"\n')
        fake_uname.chmod(0o755);fake_curl.chmod(0o755)
        self.environment = dict(os.environ,PATH=str(tools)+os.pathsep+os.environ['PATH'],STUDY_ARCHIVE=str(self.archive))

    def run_script(self):
        with tarfile.open(self.archive,'w:gz') as archive:
            archive.add(self.package,arcname=self.package.name)
        return subprocess.run(['bash',str(self.script)],env=self.environment,capture_output=True,text=True)

    def test_valid_archive_preserves_runtime_alias_and_notice(self):
        result = self.run_script();self.assertEqual(result.returncode,0,result.stderr)
        alias = self.sdk/'lib/linux-x64/libonnxruntime.so'
        self.assertTrue(alias.is_symlink());self.assertEqual(alias.read_bytes(),b'fixture library')
        self.assertEqual((self.sdk/'LICENSE').read_text(),'fixture upstream notice')

    def test_unknown_architecture_fails_without_installing_x64(self):
        self.environment['STUDY_CPU'] = 'riscv64'
        result = self.run_script();self.assertNotEqual(result.returncode,0)
        self.assertIn('Unsupported Linux architecture',result.stderr);self.assertFalse(self.sdk.exists())

    def test_incomplete_archive_leaves_existing_sdk_untouched(self):
        (self.package/'include/onnxruntime_cxx_inline.h').unlink()
        (self.sdk/'include').mkdir(parents=True)
        old = self.sdk/'include/onnxruntime_c_api.h';old.write_text('old SDK')
        result = self.run_script();self.assertNotEqual(result.returncode,0)
        self.assertIn('Incomplete',result.stderr);self.assertEqual(old.read_text(),'old SDK')
        self.assertFalse((self.sdk/'lib').exists())

    def test_copy_failure_does_not_report_ready(self):
        (self.sdk/'include/onnxruntime_c_api.h').mkdir(parents=True)
        result = self.run_script();self.assertNotEqual(result.returncode,0)
        self.assertNotIn('ready for CMake',result.stdout)


if __name__ == '__main__':unittest.main()
