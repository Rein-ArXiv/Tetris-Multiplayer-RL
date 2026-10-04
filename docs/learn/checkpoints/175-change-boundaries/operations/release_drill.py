"""Collect local evidence for an explicitly identified pair of test binaries.

This report is a teaching artifact, not authorization to deploy the product.
Only owned local executables and this checkpoint's fixed scripts are invoked.
"""
import argparse
from contextlib import redirect_stdout
import io
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
from release_evidence import evaluate
from recovery_drill import drill


def fingerprint(paths):
    digest = hashlib.sha256()
    for label, path in paths:
        # Labels and contents are framed; paths on the local disk are not identities.
        encoded = label.encode('utf-8')
        digest.update(len(encoded).to_bytes(8, 'big'))
        digest.update(encoded)
        data = path.read_bytes()
        digest.update(len(data).to_bytes(8, 'big'))
        digest.update(data)
    return digest.hexdigest()


def run_contract(argv):
    try:
        result = subprocess.run(argv, text=True, encoding='utf-8', errors='replace',
                                capture_output=True, timeout=40)
    except subprocess.TimeoutExpired:
        return 'failed', 'time budget exceeded'
    except OSError as error:
        return 'failed', 'process startup error: ' + type(error).__name__
    # Fixed commands are contract executables whose zero status means checks ran.
    # This rule must not be reused for arbitrary test runners that allow all-skip.
    status = 'passed' if result.returncode == 0 else 'failed'
    output = (result.stdout + result.stderr).encode('utf-8')
    return status, f'exit={result.returncode}; output_sha256={hashlib.sha256(output).hexdigest()}'


def collect(rules, service):
    checkpoint = Path(__file__).resolve().parents[1]
    sources = sorted((p.relative_to(checkpoint).as_posix(), p)
                     for p in checkpoint.rglob('*')
                     if p.is_file() and '__pycache__' not in p.parts)
    revision = 'checkpoint:' + fingerprint(sources)
    artifacts = [('rules', rules), ('service', service)]
    artifact = fingerprint(artifacts)
    environment = platform.system() + '/' + platform.machine()
    requirements = [dict(id='rules', scope='unit', environment=environment),
                    dict(id='accounts', scope='integration', environment=environment),
                    dict(id='display', scope='gui', environment=environment),
                    dict(id='capacity', scope='load', environment=environment)]
    records = []
    status, evidence = run_contract([str(rules)])
    records.append(dict(id='rules', scope='unit', environment=environment,
                        revision=revision, artifact_sha256=artifact,
                        status=status, evidence=evidence))
    # The integration helper owns/cleans up its service children in this process.
    # Do not wrap a child-spawning Python helper with timeout-and-kill of only its parent.
    output = io.StringIO()
    try:
        with redirect_stdout(output):
            drill(service)
        status = 'passed'
        evidence = 'output_sha256=' + hashlib.sha256(output.getvalue().encode()).hexdigest()
    except Exception as error:
        status, evidence = 'failed', 'restore drill error: ' + type(error).__name__
    records.append(dict(id='accounts', scope='integration', environment=environment,
                        revision=revision, artifact_sha256=artifact,
                        status=status, evidence=evidence))
    # Re-enumerate to notice added/deleted source files as well as changed bytes.
    after = sorted((p.relative_to(checkpoint).as_posix(), p)
                   for p in checkpoint.rglob('*')
                   if p.is_file() and '__pycache__' not in p.parts)
    if fingerprint(artifacts) != artifact or fingerprint(after) != revision.split(':', 1)[1]:
        raise RuntimeError('candidate changed during checks')
    return dict(revision=revision, artifact_sha256=artifact, requirements=requirements,
                records=records, assessment=evaluate(revision, artifact, requirements, records))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rules', required=True, type=Path)
    parser.add_argument('--service', required=True, type=Path)
    args = parser.parse_args()
    report = collect(args.rules.resolve(strict=True), args.service.resolve(strict=True))
    print(json.dumps(report, indent=2))
    # Successful demonstration deliberately reports missing GUI/load evidence.
    # Exit status here describes the executed contracts; assessment.ready has a wider scope.
    if any(r['status'] != 'passed' for r in report['records']):
        raise SystemExit(1)


if __name__ == '__main__': main()
