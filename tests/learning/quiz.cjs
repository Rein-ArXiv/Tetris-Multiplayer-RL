// Exercise the actual browser state code with old written answers and new choices.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const root=path.resolve(__dirname,'../../docs/learn');
const questions=JSON.parse(fs.readFileSync(path.join(root,'lesson-1-questions.json')));
for(const file of fs.readdirSync(path.join(root,'lessons'))) {
  questions.push(...JSON.parse(fs.readFileSync(path.join(root,'lessons',file))).questions);
}
assert.ok(questions.length >= 44);
for(const q of questions) {
  assert.deepEqual(q.options.map(o=>o.id),['a','b','c','d']);
  assert.equal(q.options.filter(o=>o.id===q.correctOptionId).length,1);
  assert.equal(new Set(q.options.map(o=>o.text)).size,4);
  assert.ok(q.explanation.trim());
}
class Element {
  constructor(dataset={}) {this.dataset=dataset;this.value='';this.checked=false;this.hidden=false;this.children=[];this.listeners={};this.textContent='';}
  addEventListener(type,fn){(this.listeners[type] ||= []).push(fn);}
  emit(type){const event={target:this};for(const fn of this.listeners[type]||[])fn(event);this.document?.emit(type,event);}
  hasAttribute(name){return name==='data-view' && this.id==='lesson-1';}
  closest(selector){const m=/^\[data-([a-z-]+)\]$/.exec(selector);if(!m)return null;const key=m[1].replace(/-([a-z])/g,(_,c)=>c.toUpperCase());return Object.hasOwn(this.dataset,key)?this:null;}
  append(...children){this.children.push(...children);}
  replaceChildren(...children){this.children=children;this.textContent='';}
  setAttribute(){} removeAttribute(){} focus(){this.focused=true;}
  querySelector(){return this.inner || {cloneNode:()=>({textContent:'Lesson',querySelectorAll:()=>[]})};}
  get text(){return this.textContent+this.children.map(c=>c.textContent).join('\n');}
}
function boot(seed, fail=false){
  const nodes=new Map();const node=id=>{if(!nodes.has(id))nodes.set(id,new Element());return nodes.get(id);};
  const radios=questions.flatMap(q=>q.options.map(o=>{const n=new Element({choice:q.id});n.value=o.id;return n;}));
  const checks=questions.map(q=>new Element({checkChoice:q.id}));
  const clears=questions.map(q=>new Element({clearChoice:q.id}));
  const feedback=Object.fromEntries(questions.map(q=>[q.id,new Element()]));
  const legacy=questions.map(q=>{const n=new Element({legacyAnswer:q.id});n.inner=new Element();return n;});
  const complete=new Element({complete:'lesson-1'});const view=new Element();view.id='lesson-1';
  const storage={raw:JSON.stringify(seed),fail,setItem(k,v){if(this.fail)throw new Error('quota');this.raw=v;},getItem(){return this.raw;}};
  nodes.set('lesson-1',view);
  const documentListeners={};
  const sandbox={console,localStorage:storage,location:{hash:'#lesson-1'},setTimeout:()=>1,clearTimeout(){},navigator:{},
    LEARNING_QUESTION_INDEX:new Map(questions.map(q=>[q.id,q])),addEventListener(){},scrollTo(){},
    document:{title:'',getElementById:node,createElement:()=>new Element(),
      addEventListener(type,fn){(documentListeners[type] ||= []).push(fn);},
      emit(type,event){for(const fn of documentListeners[type]||[])fn(event);},
      querySelector(selector){const m=/data-choice-feedback="([^"]+)"/.exec(selector);if(m)return feedback[m[1]];const r=/^\[data-choice="([^"]+)"\]$/.exec(selector);return r?radios.find(x=>x.dataset.choice===r[1]):null;},
      querySelectorAll(selector){const r=/^\[data-choice="([^"]+)"\]$/.exec(selector);if(r)return radios.filter(x=>x.dataset.choice===r[1]);return ({'[data-view]':[view],'[data-answer]':[],'[data-choice]':radios,'[data-complete]':[complete],
        '[data-check-choice]':checks,'[data-clear-choice]':clears,'[data-legacy-answer]':legacy})[selector]||[];}}};
  for(const element of [...radios,...checks,...clears,complete])element.document=sandbox.document;
  sandbox.window=sandbox;vm.createContext(sandbox);vm.runInContext(fs.readFileSync(path.join(root,'app.js'),'utf8'),sandbox);
  const select=(id,value)=>{for(const r of radios.filter(r=>r.dataset.choice===id))r.checked=r.value===value;
    radios.find(r=>r.dataset.choice===id&&r.value===value).emit('change');};
  return {node,radios,checks,clears,feedback,legacy,complete,storage,select};
}
// A future-record fixture must stay outside the authored corpus as it grows.
const futureNumber=1+Math.max(...questions.map(q=>Number(q.id.split('-')[0])));
const futureQuestion=`${futureNumber}-1`, futureLesson=`lesson-${futureNumber}`;
assert.ok(!questions.some(q=>q.id===futureQuestion));
const seed={answers:{'1-1':'이전 서술 답안 <img src=x>',[futureQuestion]:'미래 서술 답안'},complete:true,completedLessons:{[futureLesson]:true},choices:{[futureQuestion]:'c'},confirmedChoices:{[futureQuestion]:'c'}};
const h=boot(seed);
assert.equal(h.complete.checked,true);
assert.equal(h.legacy[0].inner.textContent,seed.answers['1-1']);
assert.equal(h.legacy[0].hidden,false);
assert.ok(h.legacy.slice(1).every(p=>p.hidden));
assert.ok(Object.values(h.feedback).every(p=>p.hidden));
h.checks[0].emit('click');assert.match(h.feedback['1-1'].text,/먼저/);assert.doesNotMatch(h.feedback['1-1'].text,/정답/);
// Every question reveals its matching explanation only on explicit confirmation.
for(const q of questions){
 const check=h.checks.find(n=>n.dataset.checkChoice===q.id);
 const wrong=q.options.find(o=>o.id!==q.correctOptionId).id;
 h.select(q.id,wrong);assert.equal(h.feedback[q.id].hidden,true);
 check.emit('click');assert.match(h.feedback[q.id].text,/정답이 아닙니다/);
 assert.ok(h.feedback[q.id].text.includes(q.explanation));
 assert.ok(h.feedback[q.id].text.includes(`정답 · ${q.correctOptionId.toUpperCase()}`));
 h.select(q.id,q.correctOptionId);assert.equal(h.feedback[q.id].hidden,true);
 check.emit('click');assert.match(h.feedback[q.id].text,/맞았습니다/);
}
const saved=JSON.parse(h.storage.raw);
assert.deepEqual(saved.answers,seed.answers);assert.equal(saved.choices[futureQuestion],'c');assert.equal(saved.completedLessons[futureLesson],true);
const reloaded=boot(saved);
assert.equal(reloaded.complete.checked,true,'manual completion survives reload');
reloaded.complete.checked=false;reloaded.complete.emit('change');
assert.equal(boot(JSON.parse(reloaded.storage.raw)).complete.checked,false,'unchecking completion persists');
assert.ok(Object.values(reloaded.feedback).every(p=>!p.hidden));
assert.ok(reloaded.radios.filter(r=>r.checked).length===questions.length);
reloaded.clears[0].emit('click');
assert.equal(reloaded.feedback['1-1'].hidden,true);
assert.ok(reloaded.radios.filter(r=>r.dataset.choice==='1-1').every(r=>!r.checked));
assert.equal(JSON.parse(reloaded.storage.raw).answers['1-1'],seed.answers['1-1']);
const blocked=boot(seed,true);blocked.select('1-1','b');blocked.checks[0].emit('click');
assert.match(blocked.feedback['1-1'].text,/맞았습니다/);assert.match(blocked.node('storage-status').textContent,/저장 불가/);
assert.equal(blocked.storage.raw,JSON.stringify(seed));
console.log(`${questions.length} quizzes: no early reveal, wrong/right feedback, retry, restore, clear, legacy/future preservation and unavailable storage passed`);
