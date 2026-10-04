"""Build teaching checkpoints and test actual SDL lifetime paths on Linux.

SDL's dummy video backend plus a separate LD_PRELOAD fixture exercise early
returns, explicit cleanup, and RAII cleanup. This is not native GUI validation.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/learn/checkpoints'
OUT = ROOT / 'out/learning-checkpoints'

def command(args, **kwargs):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=kwargs.pop("timeout",60), **kwargs)
    if result.returncode:
        raise RuntimeError(f"Command failed: {args}\n{result.stdout}\n{result.stderr}")
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkpoints', nargs='*')
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('This interposer test is Linux-only; other platforms need their own validation.')
    names = args.checkpoints or [p.name for p in sorted(SOURCE.iterdir()) if p.is_dir()]
    OUT.mkdir(parents=True, exist_ok=True)
    flags = shlex.split(command(['pkg-config','--cflags','--libs','sdl2']).stdout)
    probe = OUT / 'sdl_probe.so'
    command(['cc','-shared','-fPIC',str(ROOT/'tests/learning/sdl_probe.c'),*flags,'-ldl','-o',str(probe)])
    production_probe = OUT / 'current_platform_focus'
    command(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',
             str(ROOT/'tests/learning/current_platform_focus.cpp'),*flags,'-o',str(production_probe)])
    print(command([str(production_probe)]).stdout.strip())
    checked = 0
    for name in names:
        if name not in {p.name for p in SOURCE.iterdir() if p.is_dir()}:
            parser.error(f'Unknown checkpoint: {name}')
        later_checks = {
            '52-state-hash': 'hash', '53-golden-regression': 'golden',
            '54-game-adapter': 'adapter', '55-screen-state': 'screen',
            '56-color-batch': 'batch', '57-flush-boundaries': 'flush',
            '58-texture-storage': 'texture',
            '59-image-decode': 'decode',
            '60-image-handles': 'handles',
            '61-image-transform': 'transform',
            '62-rounded-corners': 'rounded',
            '63-utf8': 'utf8',
            '64-glyph': 'glyph',
            '65-atlas': 'atlas',
            '66-text-layout': 'text_layout',
            '67-font-cache': 'font_cache',
            '68-immediate-ui': 'immediate_ui',
            '69-widget-state': 'widgets',
            '70-pointer-mapping': 'pointer_mapping',
            '71-character-art': 'character_art',
            '72-idle-animation': 'idle_animation',
            '73-settings': 'settings',
            '74-pcm': 'pcm',
            '75-mp3': 'mp3',
            '76-playback': 'playback',
            '77-callback': 'callback',
            '78-mixing': 'mixing',
            '79-sound-events': 'sound_events',
            '80-voice-pool': 'voice_pool',
            '81-xaudio2': 'xaudio',
            '82-audio-failure': 'audio_failure',
            '83-sockets': 'sockets',
            '84-tcp-stream': 'tcp_stream',
            '85-framing': 'framing',
            '86-serialization': 'serialization',
            '87-partial-send': 'partial_send',
            '88-connection-lifetime': 'connection',
            '89-seed-handshake': 'seed',
            '90-input-exchange': 'lockstep',
            '91-input-delay': 'delay',
            '92-hash-audit': 'hash_audit',
            '93-thread-queues': 'threads',
            '94-heartbeat': 'heartbeat',
            '95-backpressure': 'backpressure',
            '96-round-inputs': 'round_inputs',
            '97-hash-observation': 'hash_observation',
            '98-end-negotiation': 'end_negotiation',
            '99-relay-choice': 'relay_choice',
            '100-first-admission': 'first_admission',
            '101-worker-lifetime': 'worker_lifetime',
            '102-match-queue': 'pair_queue',
            '103-room-code': 'room_code',
            '104-acceptance-lobby': 'acceptance_lobby',
            '105-forwarder': 'forwarder',
            '106-room-exit': 'room_exit',
            '107-connection-budget': 'connection_budget',
            '108-meta-boundary': 'meta_boundary',
            '109-meta-service': 'meta_service',
            '110-tables-keys': 'tables_keys',
            '111-indexes': 'indexes',
            '112-migrations': 'migrations',
            '113-transactions': 'transactions',
            '114-idempotency': 'idempotency',
            '115-progression': 'progression',
            '116-guest-account': 'guest_account',
            '117-icon-ownership': 'icon_ownership',
            '118-json-boundary': 'json_boundary',
            '119-http-failure': 'http_failure',
            '120-account-bootstrap': 'account_bootstrap',
            '121-save-uncertainty': 'save_uncertainty',
            '122-account-screen': 'account_screen',
            '123-ranking': 'ranking',
            '124-thread-measurement': 'measurement',
            '125-io-models': 'io_models',
            '126-reactor-contract': 'reactor_contract',
            '127-epoll': 'epoll',
            '128-iocp': 'iocp',
            '129-timers': 'timers',
            '130-offload': 'offload',
            '131-state-machine': 'state_machine',
            '132-backpressure': 'backpressure',
            '133-sharding': 'sharding',
            '134-threat-model': 'threat_model',
            '135-secure-connections': 'secure_connections',
            '136-wss-tunnel': 'wss_tunnel',
            '137-admission-tickets': 'admission_tickets',
            '138-async-tls': 'async_tls',
            '139-credential-crypto': 'credential_crypto',
            '140-credential-migration': 'credential_migration',
            '141-account-change': 'account_change',
            '142-recovery-journal': 'recovery_journal',
            '143-authoritative-simulation': 'authoritative_simulation',
            '144-input-pacing': 'input_pacing',
            '145-relay-ownership': 'relay_ownership',
            '146-result-notices': 'result_notices',
            '147-bot-rewards': 'bot_rewards',
            '148-abuse-review': 'abuse_review',
            '149-python-binding': 'python_binding',
            '150-observation': 'observation',
            '151-actions': 'actions',
            '152-rewards': 'rewards',
            '153-gym-environment': 'gym_environment',
            '154-versus': 'versus',
            '155-heuristic': 'heuristic',
            '156-policy-network': 'policy_network',
            '157-ppo': 'ppo',
            '158-training-checkpoint': 'training_checkpoint',
            '159-colab-workflow': 'colab_workflow',
            '160-model-zoo': 'model_zoo',
            '161-onnx-export': 'onnx_export',
            '162-cpp-inference': 'cpp_inference',
            '163-input-route': 'input_route',
            '164-bot-pacing': 'bot_pacing',
            '165-characters': 'characters',
            '166-policy-fallback': 'policy_fallback',
            '167-build-targets': 'build_targets', '168-dependencies': 'dependencies',
            '169-linux-service': 'linux_service',
            '170-windows-port': 'windows_port',
            '171-private-publication': 'private_publication',
            '172-shutdown': 'shutdown',
            '173-backup-restore': 'backup_restore',
            '174-release-evidence': 'release_evidence',
            '175-change-boundaries': 'change_boundaries',
            '176-content-extension': 'content_extension',
            '177-system-review': 'system_review',
        }
        if name in later_checks:
            script = ROOT / f'scripts/check_learning_{later_checks[name]}.py'
            print(command([sys.executable,str(script)],timeout=1200).stdout.strip())
            checked += 1
            continue
        if name == '51-rng-streams':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_streams.py')],timeout=720).stdout.strip())
            checked += 1
            continue
        if name == '50-seeded-rng':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_rng.py')],timeout=720).stdout.strip())
            checked += 1
            continue
        if name == '49-seven-bag':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_bag.py')],timeout=720).stdout.strip())
            checked += 1
            continue
        if name == '48-input-mask':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_input_mask.py')],timeout=720).stdout.strip())
            checked += 1
            continue
        if name == '47-input-edges':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_input_edges.py')],timeout=720).stdout.strip())
            checked += 1
            continue
        if name == '46-catch-up':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_catch_up.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '45-clock-accounting':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_clock_accounting.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '44-frame-loop':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_frame_loop.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '43-history':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_history.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '42-combat':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_combat.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '41-hard-drop':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_hard_drop.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '40-soft-drop':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_soft_drop.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '39-ghost':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_ghost.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '38-score':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_score.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '37-game-over':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_end.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '36-next-queue':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_queue.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '35-kicks':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_kicks.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '34-rotation':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_rotation.py')],timeout=480).stdout.strip())
            checked += 1
            continue
        if name == '33-line-clear':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_lines.py')]).stdout.strip())
            checked += 1
            continue
        if name == '32-locking':
            print(command([sys.executable,str(ROOT/'scripts/check_learning_locking.py')]).stdout.strip())
            checked += 1
            continue
        if name == '31-gravity':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_gravity.py')]).stdout.strip())
            checked += 1
            continue
        if name == '30-collision':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_collision.py')]).stdout.strip())
            checked += 1
            continue
        if name == '29-horizontal-move':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_movement.py')]).stdout.strip())
            checked += 1
            continue
        if name == '28-piece-catalog':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_catalog.py')]).stdout.strip())
            checked += 1
            continue
        if name == '27-local-piece':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_piece.py')]).stdout.strip())
            checked += 1
            continue
        if name == '26-board-render':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_board_render.py')]).stdout.strip())
            checked += 1
            continue
        if name == '25-grid':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_grid.py')]).stdout.strip())
            checked += 1
            continue
        if name == '24-letterbox':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_letterbox.py')]).stdout.strip())
            checked += 1
            continue
        if name == '23-present':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_present.py')]).stdout.strip())
            continue
        if name == '22-blending':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_blend.py')]).stdout.strip())
            continue
        if name == '21-quad':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_quad.py')]).stdout.strip())
            continue
        if name == '20-raster':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_raster.py')]).stdout.strip())
            continue
        if name == '19-coordinates':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_coordinates.py')]).stdout.strip())
            continue
        if name == '18-triangle':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_triangle.py')]).stdout.strip())
            continue
        if name == '17-program':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_program.py')]).stdout.strip())
            continue
        if name == '16-shader':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_shader.py')]).stdout.strip())
            continue
        if name == '15-vao':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_vao.py')]).stdout.strip())
            continue
        if name == '14-vbo':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_vbo.py')]).stdout.strip())
            continue
        if name == '13-vertex-data':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_mesh.py')]).stdout.strip())
            continue
        if name == '12-gl-loader':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_loader.py')]).stdout.strip())
            continue
        if name == '11-gl-context':
            print(command([sys.executable, str(ROOT/'scripts/check_learning_gl.py')]).stdout.strip())
            continue
        directory = OUT / name
        command(['cmake','-S',str(SOURCE/name),'-B',str(directory),'-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
        result = command(['cmake','--build',str(directory),'-j2'])
        if 'warning:' in result.stderr:
            raise RuntimeError(result.stderr)
        cases = [('quit',0),('window-fail',1)]
        if name == '02-window': cases += [('wait-fail',1)]
        else: cases += [('frequency-fail',1),('timed-quit',0)]
        for mode, expected in cases:
            env = {**os.environ,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(probe),'LEARN_SMOKE':mode}
            result = subprocess.run([str(directory/'tetris')], env=env, capture_output=True, text=True, timeout=5)
            assert result.returncode == expected, (name,mode,result)
            log = [line for line in result.stderr.splitlines() if line.startswith('probe:')]
            expected_log = ['probe:init'] + ([] if mode == 'window-fail' else ['probe:destroy']) + ['probe:quit']
            assert log == expected_log, (name,mode,log)
            if mode == 'timed-quit': assert 'elapsed=' in result.stdout
            checked += 1
        result = subprocess.run([str(directory/'tetris')],env={**os.environ,'SDL_VIDEODRIVER':'definitely_missing'},capture_output=True,text=True,timeout=5)
        assert result.returncode == 1 and 'init failed' in result.stderr
        checked += 1
        if int(name.split('-')[0]) >= 5:
            assert (directory/'libstudy_platform.a').exists()
            commands = json.loads((directory/'compile_commands.json').read_text())
            main_command = next(row['command'] for row in commands if row['file'].endswith('/src/main.cpp'))
            assert 'SDL2' not in main_command, main_command
        if (SOURCE/name/'platform/platform.h').exists():
            fixture = directory/'lifetime_probe'
            command(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',
                     '-I'+str(SOURCE/name), str(ROOT/'tests/learning/platform_lifetime.cpp'),
                     str(SOURCE/name/'platform/sdl.cpp'), *flags, '-o', str(fixture)])
            for mode in ['quit','window-fail','frequency-fail']:
                env = {**os.environ,'SDL_VIDEODRIVER':'dummy','LD_PRELOAD':str(probe),'LEARN_SMOKE':mode}
                result = subprocess.run([str(fixture)] + ([] if mode == 'quit' else ['failure']),
                                        env=env,capture_output=True,text=True,timeout=5)
                assert result.returncode == 0, (name, mode, result.stderr)
                log = [line for line in result.stderr.splitlines() if line.startswith('probe:')]
                expected_log = ['probe:init'] + ([] if mode == 'window-fail' else ['probe:destroy']) + ['probe:quit']
                assert log == expected_log * 2, (name, mode, log)
                checked += 1
        if name in {'07-keys', '08-text', '09-backends', '10-win32'}:
            fixture = directory/'input_probe'
            command(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',
                     *(['-DLEARN_TEXT'] if name in {'08-text', '09-backends', '10-win32'} else []),
                     '-I'+str(SOURCE/name), str(ROOT/'tests/learning/platform_input.cpp'),
                     str(SOURCE/name/'platform/sdl.cpp'), *flags, '-o', str(fixture)])
            env = {key:value for key,value in os.environ.items() if key not in {'LD_PRELOAD','LEARN_SMOKE'}}
            command([str(fixture)], env={**env,'SDL_VIDEODRIVER':'dummy'})
            checked += 1
        if name in {'09-backends', '10-win32'}:
            script_dir = OUT/(name+'-scripted')
            command(['cmake','-S',str(SOURCE/name),'-B',str(script_dir),
                     '-DSTUDY_PLATFORM=SCRIPTED','-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic',
                     '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
            command(['cmake','--build',str(script_dir),'-j2'])
            result = command([str(script_dir/'tetris')])
            assert result.stdout.splitlines() == [
                'left pressed', 'text byte=65', 'left released', 'text byte=66', 'text byte=67',
                'text dropped=0', 'left held=0 right held=0',
                'elapsed=1.000 frames=4 events=4 last_dt=0.250000'], result.stdout
            commands = (script_dir/'compile_commands.json').read_text()
            assert '/platform/sdl.cpp' not in commands and 'SDL2' not in commands
            fixture = script_dir/'script_probe'
            command(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',
                     '-I'+str(SOURCE/name),str(ROOT/'tests/learning/platform_scripted.cpp'),
                     str(SOURCE/name/'platform/scripted.cpp'),'-o',str(fixture)])
            command([str(fixture)])
            bad = subprocess.run(['cmake','-S',str(SOURCE/name),'-B',str(OUT/(name+'-invalid')),
                                  '-DSTUDY_PLATFORM=UNKNOWN'],text=True,capture_output=True,timeout=60)
            assert bad.returncode != 0 and 'Unknown STUDY_PLATFORM' in bad.stderr
            checked += 3
        if name == '10-win32':
            wrong_os = subprocess.run(['cmake','-S',str(SOURCE/name),'-B',str(OUT/'10-wrong-os'),
                                       '-DSTUDY_PLATFORM=WIN32'],text=True,capture_output=True,timeout=60)
            assert wrong_os.returncode != 0 and 'requires a Windows target' in wrong_os.stderr
            checked += 1
        print(f'{name}: build, lifetime and applicable input checks passed')
    print(f'{checked} base input/lifetime paths passed; GL/loader checks are reported separately above. Native GUI and other OSes not tested.')

if __name__ == '__main__':
    main()
