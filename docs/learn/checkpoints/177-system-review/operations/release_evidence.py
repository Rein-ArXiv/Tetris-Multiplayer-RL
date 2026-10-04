'''Educational release-evidence evaluator (standard library only).

This module classifies declared evidence records against a candidate
revision and artifact digest. It does NOT execute code, open evidence
strings, or verify authenticity. Records are declarations, not proof.
'''

from __future__ import annotations

import re
from collections.abc import Mapping, Sequence
from typing import Any

RECORD_STATUSES = frozenset({'passed', 'failed', 'skipped', 'not_run'})
SCOPES = frozenset({'unit', 'integration', 'gui', 'load'})
DECISION_STALE = 'stale'
DECISION_NOT_RUN = 'not_run'
SHA256_RE = re.compile(r'^[0-9a-f]{64}$')


def _require_text(value: Any, field: str) -> str:
    '''Return value when it is nonempty text; otherwise raise ValueError.'''
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f'{field} must be nonempty text')
    return value


def _require_sha256(value: Any, field: str) -> str:
    '''Return value when it is exactly 64 lowercase hex chars.'''
    if not isinstance(value, str) or not SHA256_RE.fullmatch(value):
        raise ValueError(f'{field} must be exactly 64 lowercase hex chars')
    return value


def evaluate(candidate_revision: str, artifact_sha256: str,
             requirements: Sequence[Mapping[str, str]],
             records: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    '''Classify records against the candidate and report readiness.

    Returns {'decisions': [...], 'ready': bool}. Decisions preserve the
    order of requirements. Extra records never satisfy a requirement.
    '''
    # Validate the candidate identity.
    candidate_revision = _require_text(candidate_revision, 'candidate_revision')
    artifact_sha256 = _require_sha256(artifact_sha256, 'artifact_sha256')

    # Validate requirements: nonempty and unique.
    if not isinstance(requirements, (list, tuple)):
        raise ValueError('requirements must be a list')
    seen_req = set()
    clean_requirements: list[dict[str, str]] = []
    for req in requirements:
        if not isinstance(req, Mapping) or set(req) != {'id', 'environment', 'scope'}:
            raise ValueError('requirement needs id, environment and scope')
        ident = _require_text(req['id'], 'requirement id')
        environment = _require_text(req['environment'], 'required environment')
        scope = req['scope']
        if not isinstance(scope, str) or scope not in SCOPES:
            raise ValueError('invalid required scope')
        if ident in seen_req:
            raise ValueError(f'duplicate requirement id: {ident!r}')
        seen_req.add(ident)
        clean_requirements.append(dict(id=ident, environment=environment, scope=scope))

    # Validate records and index them by id.
    if not isinstance(records, (list, tuple)):
        raise ValueError('records must be a list')
    required_keys = {'id', 'revision', 'artifact_sha256',
                     'environment', 'scope', 'status', 'evidence'}
    by_id: dict[str, dict[str, Any]] = {}
    for index, record in enumerate(records):
        if not isinstance(record, Mapping):
            raise ValueError(f'record {index} must be a mapping')
        missing = required_keys - set(record)
        if missing:
            raise ValueError(f'record {index} missing keys: {sorted(missing)}')

        rec_id = _require_text(record['id'], f'record {index} id')
        if rec_id in by_id:
            raise ValueError(f'duplicate record id: {rec_id!r}')

        revision = _require_text(record['revision'], f'record {index} revision')
        digest = _require_sha256(record['artifact_sha256'],
                                 f'record {index} artifact_sha256')
        environment = _require_text(record['environment'],
                                    f'record {index} environment')

        scope = record['scope']
        if not isinstance(scope, str) or scope not in SCOPES:
            raise ValueError(f'record {index} has invalid scope: {scope!r}')

        status = record['status']
        if not isinstance(status, str) or status not in RECORD_STATUSES:
            raise ValueError(f'record {index} has invalid status: {status!r}')

        # Evidence must be nonempty whenever a verdict is claimed.
        evidence = record['evidence']
        if status in {'passed', 'failed'}:
            evidence = _require_text(evidence, f'record {index} evidence')
        elif not isinstance(evidence, str):
            raise ValueError(f'record {index} evidence must be text')

        by_id[rec_id] = {
            'id': rec_id,
            'revision': revision,
            'artifact_sha256': digest,
            'environment': environment,
            'scope': scope,
            'status': status,
            'evidence': evidence,
        }

    # Classify each required check in order; never substitute extras.
    decisions: list[dict[str, Any]] = []
    for requirement in clean_requirements:
        req_id = requirement['id']
        record = by_id.get(req_id)
        if record is None:
            decisions.append({'id': req_id, 'status': DECISION_NOT_RUN,
                              'reason': 'missing required check'})
            continue
        # A declaration only applies to the exact candidate it names.
        if (record['revision'] != candidate_revision
                or record['artifact_sha256'] != artifact_sha256):
            decisions.append({'id': req_id, 'status': DECISION_STALE,
                              'reason': 'revision or artifact digest mismatch'})
            continue
        if any(record[field] != requirement[field] for field in ('environment', 'scope')):
            decisions.append({'id': req_id, 'status': 'mismatch',
                              'reason': 'environment or scope mismatch'})
            continue
        decisions.append({'id': req_id, 'status': record['status'],
                          'reason': 'declared evidence for candidate'})

    # Ready only for a nonempty requirement set where all pass.
    ready = bool(clean_requirements) and all(
        d['status'] == 'passed' for d in decisions)
    return {'decisions': decisions, 'ready': ready}
