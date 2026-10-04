"""Exercise the actual client asset commands without requiring a GPU/SDL SDK."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class ClientAssets(unittest.TestCase):
    def run_command(self, args, **kwargs):
        result = subprocess.run(args, capture_output=True, text=True, **kwargs)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def exercise(self, generator, configs):
        text = (ROOT / 'CMakeLists.txt').read_text()
        start = text.index('    # Runtime resources are a prerequisite')
        end = text.index('    add_dependencies(tetris copy_assets)', start)
        block = text[start:end] + '    add_dependencies(tetris copy_assets)\n'
        with tempfile.TemporaryDirectory(prefix='study assets ') as temp:
            source = Path(temp) / 'source space';source.mkdir()
            build = Path(temp) / 'build space'
            for name in ('Font', 'Sounds', 'assets'):
                (source / name).mkdir();(source / name / 'marker.txt').write_text(name)
            (source / 'main.cpp').write_text('int main() { return 0; }\n')
            (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.15)
project(asset_contract LANGUAGES CXX)
add_executable(tetris main.cpp)
add_executable(unrelated EXCLUDE_FROM_ALL main.cpp)
''' + block)
            self.run_command(['cmake', '-S', str(source), '-B', str(build), '-G', generator])
            for config in configs:
                command = ['cmake', '--build', str(build), '--target', 'tetris', '--config', config]
                self.run_command(command)
                output = build / config if len(configs) > 1 else build
                for name in ('Font', 'Sounds', 'assets'):
                    self.assertEqual((output / name / 'marker.txt').read_text(), name)
                self.assertFalse((output / 'model').exists())
                self.assertFalse((output / ('unrelated.exe' if os.name == 'nt' else 'unrelated')).exists())
                # The executable is up to date, but a changed/deleted resource must refresh.
                (source / 'Font' / 'marker.txt').write_text('changed')
                shutil.rmtree(output / 'Sounds')
                self.run_command(command)
                self.assertEqual((output / 'Font' / 'marker.txt').read_text(), 'changed')
                self.assertEqual((output / 'Sounds' / 'marker.txt').read_text(), 'Sounds')
                (source / 'Font' / 'marker.txt').write_text('Font')
                self.run_command([str(output / ('tetris.exe' if os.name == 'nt' else 'tetris'))])

    def test_single_configuration(self):
        self.exercise('Ninja', ['Release'])

    def test_multiple_configurations(self):
        self.exercise('Ninja Multi-Config', ['Debug', 'Release'])

if __name__ == '__main__':
    unittest.main()
