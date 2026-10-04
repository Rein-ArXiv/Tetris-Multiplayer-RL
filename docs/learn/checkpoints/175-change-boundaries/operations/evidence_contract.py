"""Contract cases: absence, stale candidates, wrong coverage and malformed evidence."""
from copy import deepcopy
from release_evidence import evaluate


def main():
    digest = 'ab' * 32
    required = [dict(id='service', scope='integration', environment='target/arch')]
    row = dict(id='service', revision='candidate', artifact_sha256=digest,
               environment='target/arch', scope='integration', status='passed', evidence='owned run log')
    def assess(rows, requirements=required):return evaluate('candidate', digest, requirements, rows)
    assert assess([row])['ready']
    assert not assess([row], [])['ready']
    assert assess([])['decisions'][0]['status'] == 'not_run'
    for field, value, status in [('revision','old','stale'),('artifact_sha256','cd'*32,'stale'),
                                 ('scope','unit','mismatch'),('environment','different/arch','mismatch'),
                                 ('status','skipped','skipped'),('status','failed','failed'),
                                 ('status','not_run','not_run')]:
        changed = dict(row, **{field:value});result = assess([changed])
        assert not result['ready'] and result['decisions'][0]['status'] == status
    extra = dict(row, id='unrelated')
    assert not assess([extra])['ready']
    for records in ([row,row],[dict(row,artifact_sha256=digest+'\n')],
                    [dict(row, scope=[])],[dict(row,status={})],[dict(row,evidence=' ')],
                    [dict(row,environment='')],[{}]):
        try:assess(records)
        except ValueError:pass
        else:raise AssertionError('malformed declaration accepted')
    before = deepcopy([row]);assess(before);assert before == [row]
    # The collector identifies source files by content, not a potentially dirty git HEAD.
    print('Evidence coverage, stale inputs, malformed declarations and empty policy rejected')


if __name__ == '__main__': main()
