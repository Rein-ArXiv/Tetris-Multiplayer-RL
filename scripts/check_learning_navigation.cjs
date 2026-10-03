/* DOM-only contracts. No browser, display, remote request, or screenshot.
   Run with NODE_PATH pointing to an installed jsdom (kept outside the project). */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { JSDOM, VirtualConsole } = require('jsdom');
const root = process.argv[2] ? path.resolve(process.argv[2]) : path.resolve(__dirname, '../docs/learn');
const read = name => fs.readFileSync(path.join(root, name), 'utf8');
const stored = {
  answers: {'1-1': '기존 서술 답안', '999-1': '보존할 미래 답안'},
  choices: {'44-2': 'c', '999-1': 'b'}, confirmedChoices: {'44-2':'c', '999-1':'b'},
  completedLessons: {'lesson-44': true, 'lesson-999': true}
};
function boot(hash, url='https://example.test/project/learn/', blockedStorage=false, reading={location:'lesson-20--s3',theme:'dark'}, progress=stored) {
  const errors = [];
  const vc = new VirtualConsole(); vc.on('jsdomError', error => errors.push(error.message));
  const dom = new JSDOM(read('index.html'), {url: url+hash, runScripts:'outside-only', virtualConsole:vc});
  const w=dom.window, d=w.document;
  w.scrollTo=()=>{}; w.scrollBy=()=>{}; w.HTMLElement.prototype.scrollIntoView=function(){}; w.HTMLElement.prototype.scrollTo=function(){};
  w.matchMedia=()=>({matches:false,addEventListener(){}});
  w.LEARNING_SITE={mode:'static'};
  let writes=0;
  const data = new Map([
    ['tetris-learning-v1', JSON.stringify(progress)],
    ['tetris-learning-reading-v1', JSON.stringify(reading)]
  ]);
  Object.defineProperty(w,'localStorage',{value:{
    getItem(k){if(blockedStorage)throw Error('storage disabled');return data.get(k)||null;},
    setItem(k,v){if(blockedStorage)throw Error('storage disabled');++writes;data.set(k,v);}
  }});
  let copied='';
  Object.defineProperty(w.navigator,'clipboard',{value:{writeText:async text=>{copied=text;}}});
  for (const name of ['vendor/highlight.min.js','library.js','lessons.js','course.js','app.js','usability.js','reader.js']) w.eval(read(name));
  const route = hash => {w.history.replaceState(null,'',hash);w.dispatchEvent(new w.HashChangeEvent('hashchange'));};
  const count = () => d.querySelectorAll('article[data-view][id^="lesson-"]').length;
  return {dom,w,d,errors,route,count,data,writes:()=>writes,copied:()=>copied};
}
(async()=>{
  const t=boot('#lesson-44--quiz'); const {d,w,route}=t;
  assert.equal(t.count(),1); assert(d.getElementById('lesson-44--quiz').open);
  assert.equal(d.getElementById('lesson-1'),null); assert.equal(d.getElementById('lesson-43'),null);
  assert(d.querySelector('[data-choice="44-2"][value="c"]').checked);
  assert(!d.querySelector('[data-choice-feedback="44-2"]').hidden);
  assert(d.querySelector('[data-complete="lesson-44"]').checked);
  assert.match(d.getElementById('resume-reading').textContent,/20차시/);
  assert.equal(d.documentElement.dataset.theme,'dark');
  // Unfinished lessons need no status badge; completion remains visible and reversible.
  const status = id => d.querySelector(`[data-page="${id}"] .nav-status`);
  assert(!d.getElementById('course-menu').textContent.includes('학습 가능'));
  assert(status('lesson-1').hidden); assert.equal(status('lesson-1').textContent,'');
  assert(status('lesson-2').hidden);
  assert(!status('lesson-44').hidden); assert.equal(status('lesson-44').textContent,'✓');
  assert.equal(status('lesson-44').getAttribute('aria-label'),'학습 완료');
  assert.equal(status('lesson-44').getAttribute('role'),'img');
  const resume=d.getElementById('resume-reading');
  assert.equal(resume.hash,'#lesson-20--s3');
  route('#lesson-44--s2');assert.equal(resume.hash,'#lesson-44--quiz');
  route('#content');assert.equal(resume.hash,'#lesson-44--quiz');
  route('#course-sidebar');assert.equal(resume.hash,'#lesson-44--quiz');
  route('#guide');assert.equal(resume.hash,'#lesson-44--s2');
  route('#content');assert.equal(resume.hash,'#lesson-44--s2');
  route('#lesson-44--bad');assert.equal(resume.hash,'#lesson-44--s2');
  assert.equal(JSON.parse(t.data.get('tetris-learning-reading-v1')).location,'lesson-44');
  route('#lesson-44--quiz');assert.equal(resume.hash,'#lesson-44');
  const course=w.LEARNING_COURSE;
  for(const invalid of ['lesson-44--s0','lesson-44--s999','lesson-44--constructor','lesson-1--s1','lesson-999','lesson-44--quiz--bad'])assert.equal(course.anchorLabel(invalid),null,invalid);
  for(const good of ['lesson-44','lesson-44--s1','lesson-1--cs'])assert(course.anchorLabel(good),good);
  const available=[...d.querySelectorAll('[data-page^="lesson-"]')].map(x=>x.dataset.page);
  assert.equal(available.length,w.LEARNING_LESSONS.lessons.length+1);
  for(const id of available) assert(d.querySelector(`#roadmap a[href="#${id}"]`),`planned link ${id}`);
  const search=d.getElementById('lesson-search'); search.value=course.title('lesson-20');search.dispatchEvent(new w.Event('input',{bubbles:true}));
  assert(!d.querySelector('[data-page="lesson-20"]').hidden);
  route('#lesson-44--s2'); const old=d.getElementById('lesson-44');
  route('#lesson-44--s3');assert.equal(d.getElementById('lesson-44'),old);
  route('#lesson-2--quiz');assert.equal(t.count(),1);assert(!old.isConnected);
  const first=d.querySelector('[data-choice="2-1"]');first.checked=true;first.dispatchEvent(new w.Event('change',{bubbles:true}));
  d.querySelector('[data-check-choice="2-1"]').click();
  assert(!d.querySelector('[data-choice-feedback="2-1"]').hidden);
  const complete=d.querySelector('[data-complete="lesson-2"]');complete.checked=true;complete.dispatchEvent(new w.Event('change',{bubbles:true}));
  route('#lesson-3');route('#lesson-2--quiz');
  assert(d.querySelector(`[data-choice="2-1"][value="${first.value}"]`).checked);
  assert(!d.querySelector('[data-choice-feedback="2-1"]').hidden);
  assert(d.querySelector('[data-complete="lesson-2"]').checked);
  assert(!status('lesson-2').hidden); assert.equal(status('lesson-2').textContent,'✓');
  const clearComplete=d.querySelector('[data-complete="lesson-2"]');
  clearComplete.checked=false;clearComplete.dispatchEvent(new w.Event('change',{bubbles:true}));
  assert(status('lesson-2').hidden); assert.equal(status('lesson-2').textContent,'');
  const before=t.writes();d.querySelector('[data-clear-choice="2-1"]').click();assert.equal(t.writes()-before,1,'no duplicate event listeners');
  assert(d.querySelector('[data-choice-feedback="2-1"]').hidden);
  d.querySelector('[data-check-choice="2-1"]').click();assert.match(d.querySelector('[data-choice-feedback="2-1"]').textContent,/먼저 답/);
  assert.equal(d.activeElement.dataset.choice,'2-1');
  const quiz=d.getElementById('lesson-2--quiz');quiz.open=false;d.querySelector('a[href="#lesson-2--quiz"]').click();assert(quiz.open);
  route('#lesson-1--quiz');assert.equal(t.count(),1);
  assert.match(d.querySelector('[data-legacy-answer-value="1-1"]').textContent,/기존 서술/);
  assert.match(d.querySelector('button[data-source]').textContent,/배포본/);
  const codeCopy=d.querySelector('[data-copy]');codeCopy.click();await Promise.resolve();assert(t.copied().length>10);
  d.querySelector('[data-copy-answers]').click();await Promise.resolve();assert.match(t.copied(),/기존 서술/);
  for(const id of available){route('#'+id);assert.equal(t.count(),1);assert.equal(d.getElementById(id).hidden,false);assert.equal(d.querySelectorAll('[id]').length,new Set([...d.querySelectorAll('[id]')].map(n=>n.id)).size,'unique IDs');}
  const source=d.querySelector('button[data-source]');source.click();await new Promise(r=>setTimeout(r,0));
  assert.equal(d.getElementById('reference-reader').hidden,false);assert(d.getElementById('reader-body').textContent.length>0);
  if(w.LEARNING_LIBRARY.sources['web/ranking/index.html']) {
    const htmlSource=d.createElement('button');htmlSource.dataset.source='web/ranking/index.html';
    htmlSource.textContent='HTML source';d.body.append(htmlSource);htmlSource.click();
    await new Promise(r=>setTimeout(r,0));
    const sourceBody=d.getElementById('reader-body');
    assert.match(sourceBody.textContent,/<script>/,'HTML is visible as source text');
    assert.equal(sourceBody.querySelector('script,style,iframe,img'),null,'HTML source does not become active markup');
    d.getElementById('reader-copy').click();await Promise.resolve();
    assert.equal(t.copied(),w.LEARNING_LIBRARY.sources['web/ranking/index.html'].text);
    htmlSource.remove();
  }
  route('#lesson-1');d.getElementById('reader-close').click();
  assert.equal(d.getElementById('reference-reader').hidden,true);
  assert.equal(d.activeElement.id,'content','closing a reference after lesson replacement restores focus');
  route('#roadmap');assert.equal(t.count(),0);assert.equal(d.getElementById('roadmap').hidden,false);
  route('#content');assert.equal(d.getElementById('roadmap').hidden,false);
  route('#lesson-999');assert.equal(t.count(),1);assert.equal(d.getElementById('lesson-1').hidden,false);
  const finishFirst=d.querySelector('[data-complete="lesson-1"]');
  finishFirst.checked=true;finishFirst.dispatchEvent(new w.Event('change',{bubbles:true}));
  const saved=JSON.parse(t.data.get('tetris-learning-v1'));
  assert.equal(saved.completedLessons['lesson-1'],true);
  assert.equal(saved.completedLessons['lesson-2'],false);
  assert.equal(saved.answers['999-1'],stored.answers['999-1']);assert.equal(saved.choices['999-1'],'b');assert.equal(saved.confirmedChoices['999-1'],'b');assert(saved.completedLessons['lesson-999']);
  const readingSaved=JSON.parse(t.data.get('tetris-learning-reading-v1'));
  assert.notEqual(readingSaved.previousLocation,readingSaved.location);
  assert.deepEqual(t.errors,[]);t.dom.window.close();
  const restored=boot('#'+readingSaved.location,'https://example.test/project/learn/',false,readingSaved,saved);
  assert.equal(restored.d.getElementById('resume-reading').hash,'#'+readingSaved.previousLocation);
  assert(!restored.d.getElementById('resume-reading').hidden);
  assert(restored.d.querySelector('[data-complete="lesson-1"]').checked);
  assert.equal(restored.d.getElementById('lesson-status').textContent,'✓');
  restored.route('#lesson-2--quiz');
  assert(!restored.d.querySelector('[data-complete="lesson-2"]').checked);
  assert(restored.d.querySelector('[data-lesson-status="lesson-2"]').hidden);
  assert.deepEqual(restored.errors,[]);restored.dom.window.close();
  for(const hash of ['#content','#course-sidebar','#lesson-1--cs','#lesson-2--s1']){
    const fresh=boot(hash,'file:///course/index.html',true);
    assert(fresh.d.getElementById('resume-reading').hidden);
    assert.equal(fresh.count(),1);assert.match(fresh.d.getElementById('storage-status').textContent,/저장 불가/);assert.deepEqual(fresh.errors,[]);fresh.dom.window.close();
  }
  console.log('DOM contracts passed: all published lessons; single mount, anchors, metadata, search, saved answers, delegation, copies, static references, file URL/storage failure.');
})().catch(error=>{console.error(error);process.exitCode=1;});
