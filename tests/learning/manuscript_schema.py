"""Reject malformed code-block data before it reaches the HTML reader."""
from copy import deepcopy
import json
from pathlib import Path
import sys
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import build_learning_lessons as compiler
path=ROOT/'docs/learn/lessons/036.json'
original=Path.read_text
manuscript=json.loads(original(path))
invalid=[{'path':'src/sim_game.h'},None,['not a code object'],
         [{'label':'ambiguous','language':'cpp','file':'x','text':'x'}],
         [{'label':'missing source','language':'cpp'}],
         [{'label':'bad text','language':'cpp','text':123}]]
for value in invalid:
    item=deepcopy(manuscript);item['sections'][0]['codes']=value
    replacement=json.dumps(item,ensure_ascii=False)
    def read(self,*args,**kwargs):
        return replacement if self==path else original(self,*args,**kwargs)
    with patch.object(Path,'read_text',read):
        try: compiler.build()
        except ValueError as error:
            if not any(message in str(error) for message in ['Code block','code source']):raise
        else:raise AssertionError('Malformed code data was accepted')
print('Six malformed manuscript code-block cases rejected before HTML generation')

# A missing practice object previously passed compilation, then aborted course.js
# before the lesson and its navigation entry could be inserted into the DOM.
for value in [None, {}, {'steps':'one string','expected':'result','failureChecks':['check']},
              {'steps':['step'],'expected':'result','failureChecks':[]}]:
    item=deepcopy(manuscript);item['practice']=value
    replacement=json.dumps(item,ensure_ascii=False)
    def read(self,*args,**kwargs):
        return replacement if self==path else original(self,*args,**kwargs)
    with patch.object(Path,'read_text',read):
        try: compiler.build()
        except ValueError as error:
            if 'practice data' not in str(error):raise
        else:raise AssertionError('Malformed practice data was accepted')
print('Four malformed practice cases rejected before they can abort the reader')
