"""Package selection and failure propagation using local command fixtures."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]

class Release(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='linux release ');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);(self.root/'scripts').mkdir()
        for name in ('release_linux.sh','release_server_linux.sh','release_linux_common.sh','backup_meta_db.sh','backup_meta_db.py'):
            shutil.copyfile(ROOT/'scripts'/name,self.root/'scripts'/name)
        for name in ('assets','Font','Sounds'):
            (self.root/name).mkdir();(self.root/name/'marker').write_text('resource')
        self.tools=self.root/'tools';self.tools.mkdir()
        self.command('uname','#!/bin/sh\nif [ "$1" = -s ]; then echo "${TEST_OS:-Linux}"; else echo "${TEST_CPU:-x86_64}"; fi\n')
        self.command('nproc','#!/bin/sh\necho 2\n')
        self.command('ldd','#!/bin/sh\nexit 0\n')
        self.command('patchelf','#!/bin/sh\nexit "${TEST_PATCH_RC:-0}"\n')
        self.command('cmake', '''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
args=sys.argv[1:]
root=Path(os.environ['TEST_ROOT'])
with (root/'commands.jsonl').open('a') as stream:stream.write(json.dumps(args)+'\\n')
if '--build' in args:
 build=Path(args[args.index('--build')+1]);build.mkdir(exist_ok=True)
 for name in args[args.index('--target')+1:]:
  (build/name).write_text('fixture executable')
''')
        self.env=dict(os.environ,PATH=str(self.tools)+os.pathsep+os.environ['PATH'],TEST_ROOT=str(self.root),BOT='0',WSS='0')
        for key in ('ORT_ROOT','DEBUG_UI','NET_TRACE'):self.env.pop(key,None)
        self.sdk=self.root/'SDK with spaces';folder=self.sdk/'lib/linux-x64';folder.mkdir(parents=True)
        (folder/'libonnxruntime.so.1').write_bytes(b'fixture runtime')
        (folder/'libonnxruntime.so').symlink_to('libonnxruntime.so.1')

    def command(self,name,content):
        path=self.tools/name;path.write_text(content);path.chmod(0o700)

    def invoke(self,server=False):
        name='release_server_linux.sh' if server else 'release_linux.sh'
        return subprocess.run(['bash',str(self.root/'scripts'/name)],cwd=self.root,env=self.env,capture_output=True,text=True)

    def test_enabled_aliases_select_same_build_and_bundle(self):
        for server in (False,True):
            with self.subTest(server=server):
                self.env.update(BOT='oN',WSS='TrUe',ORT_ROOT=str(self.sdk))
                result=self.invoke(server);self.assertEqual(result.returncode,0,result.stderr)
                bundle=self.root/'dist'/('tetris-server-linux-x64' if server else 'tetris-linux-x64')
                runtime=bundle/'lib/libonnxruntime.so'
                self.assertTrue(runtime.is_symlink());self.assertEqual(runtime.read_bytes(),b'fixture runtime')
                if server:self.assertTrue((bundle/'tetris_wss_gateway').is_file())
                commands=[json.loads(line) for line in (self.root/'commands.jsonl').read_text().splitlines()]
                configure=commands[-2]
                self.assertIn('-DTETRIS_BUILD_BOT=1',configure)
                self.assertIn('-DTETRIS_BUILD_WSS=1',configure)
                self.assertIn('-DTETRIS_ORT_ROOT='+str(self.sdk),configure)

    def test_disabled_alias_excludes_gateway_and_runtime(self):
        self.env.update(BOT='OFF',WSS='no')
        result=self.invoke(True);self.assertEqual(result.returncode,0,result.stderr)
        bundle=self.root/'dist/tetris-server-linux-x64'
        self.assertFalse((bundle/'tetris_wss_gateway').exists())
        self.assertFalse((bundle/'lib/libonnxruntime.so').exists())

    def test_invalid_options_and_host_fail_before_build_or_bundle(self):
        for option,value in [('BOT','maybe'),('WSS','banana'),('TEST_CPU','aarch64'),('TEST_OS','Darwin')]:
            for server in (False,True):
                with self.subTest(option=option,server=server):
                    saved=dict(self.env);self.env[option]=value
                    result=self.invoke(server);self.assertNotEqual(result.returncode,0)
                    self.assertFalse((self.root/'commands.jsonl').exists())
                    self.assertFalse((self.root/'dist').exists());self.env=saved

    def test_missing_required_runtime_cannot_report_success(self):
        self.env.update(BOT='YES',ORT_ROOT=str(self.root/'missing SDK'))
        for server in (False,True):
            with self.subTest(server=server):
                result=self.invoke(server);self.assertNotEqual(result.returncode,0)
                self.assertIn('Required ONNX Runtime missing',result.stderr)
                self.assertNotIn('Done:',result.stdout)
                self.assertFalse(list((self.root/'dist').glob('*.tar.gz')))

    def test_failed_requested_rpath_patch_is_fatal(self):
        self.env['TEST_PATCH_RC']='7'
        result=self.invoke();self.assertEqual(result.returncode,7)
        self.assertNotIn('Done:',result.stdout)
        self.assertFalse(list((self.root/'dist').glob('*.tar.gz')))

if __name__=='__main__':unittest.main()
