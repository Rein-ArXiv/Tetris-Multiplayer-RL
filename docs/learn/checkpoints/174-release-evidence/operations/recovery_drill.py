"""Use only generated identities and caller-owned temporary DBs for a restore drill."""
import argparse
from contextlib import closing
import http.client
import json
from pathlib import Path
import secrets
import sqlite3
import tempfile
from local_service import local_service
from sqlite_snapshot import snapshot


def exchange(base, method, route, body=None, token=None):
    port = int(base.rsplit(':', 1)[1])
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
    headers = {'Content-Type': 'application/json'}
    if token: headers['Authorization'] = 'Bearer ' + token
    try:
        payload = json.dumps(body) if body is not None else None
        connection.request(method, route, payload, headers)
        response = connection.getresponse()
        return response.status, json.loads(response.read(8192))
    finally:
        connection.close()


def verify(condition, message):
    if not condition: raise RuntimeError(message)


def drill(binary):
    with tempfile.TemporaryDirectory(prefix='study173-') as directory:
        folder = Path(directory)
        live = folder / 'live' / 'accounts.db'
        live.parent.mkdir()
        with closing(sqlite3.connect(live)) as setup:
            verify(setup.execute('PRAGMA journal_mode=WAL').fetchone() == ('wal',),
                   'WAL setup failed')
        early = folder / 'snapshots' / 'before-rotation.db'
        latest = folder / 'snapshots' / 'after-rotation.db'
        with local_service(binary, live) as base:
            status, account = exchange(base, 'POST', '/study/v1/guest', {})
            verify(status == 201, 'guest creation failed')
            old = account['token']
            status, expected = exchange(base, 'GET', '/study/v1/inventory', token=old)
            verify(status == 200, 'inventory read failed')
            with closing(sqlite3.connect(live)) as reader:
                verify(reader.execute('PRAGMA journal_mode').fetchone() == ('wal',), 'expected live WAL')
            snapshot(live, early)
            new = secrets.token_hex(16)
            recovery = 'rc1.' + secrets.token_hex(32)
            status, changed = exchange(base, 'POST', '/study/v1/account/rotate', {
                'credential': old, 'next_token': new, 'next_recovery': recovery})
            verify(status == 200 and changed['player_id'] == account['player_id'], 'rotation failed')
            verify(exchange(base, 'GET', '/study/v1/me', token=old)[0] == 401, 'old token still active')
            verify(exchange(base, 'GET', '/study/v1/inventory', token=new) == (200, expected),
                   'rotation changed inventory')
            snapshot(live, latest)
        # Restore by copying a checked snapshot to a NEW database location.
        # Reopening the backup itself would change the evidence under inspection.
        for name, source, accepted, rejected in (
            ('recent', latest, new, old), ('historical', early, old, new)):
            restored = folder / name / 'accounts.db'
            snapshot(source, restored)
            with local_service(binary, restored) as base:
                verify(exchange(base, 'GET', '/study/v1/inventory', token=accepted) == (200, expected),
                       'restored identity/inventory mismatch')
                verify(exchange(base, 'GET', '/study/v1/me', token=rejected)[0] == 401,
                       'unexpected credential accepted')
            print(name + ': identity/inventory match; expected credential version observed')
        print('Historical restore reactivated the old credential; integrity alone cannot certify revocations.')
        # TemporaryDirectory removes only this drill's synthetic state.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    args = parser.parse_args()
    drill(args.binary.resolve(strict=True))


if __name__ == '__main__': main()
