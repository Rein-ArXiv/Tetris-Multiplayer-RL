/* Render locally reviewed lecture data before app.js binds navigation/storage. */
(() => {
  'use strict';
  const bundle = window.LEARNING_LESSONS;
  if (!bundle) return;
  const main = document.getElementById('content');
  const nav = document.getElementById('lesson-navigation');
  const el = (tag, text, cls) => {
    const node = document.createElement(tag);
    if (text !== undefined) node.textContent = text;
    if (cls) node.className = cls;
    return node;
  };
  const firstLessonId = 'lesson-1';
  // Keep the reviewed lesson-1 markup as a template, then detach it so only the
  // currently selected lesson can exist in the DOM.
  const firstTemplate = document.getElementById(firstLessonId)?.cloneNode(true) || null;
  document.getElementById(firstLessonId)?.remove();
// Question metadata is independent of the mounted article.
const questionIndex = new Map();
window.LEARNING_QUESTION_INDEX = questionIndex;
for (const q of bundle.firstQuestions || []) questionIndex.set(q.id, q);
for (const lesson of bundle.lessons) for (const q of lesson.questions) questionIndex.set(q.id, q);

/* Native choices; feedback is populated only after the user confirms. */
const questionNode = (q) => {
  const item = el('div', undefined, 'question question--choice');
  item.dataset.question = q.id;
  questionIndex.set(q.id, q);

  const fieldset = el('fieldset', undefined, 'choice');
  fieldset.append(el('legend', `${q.id} · ${q.question}`));

  const options = el('div', undefined, 'choice__options');
  q.options.forEach((option, index) => {
    const label = el('label', undefined, 'choice__option');
    const input = el('input');
    input.type = 'radio';
    input.name = `choice-${q.id}`;                 // native group per question
    input.id = `choice-${q.id}-${option.id}`;
    input.value = option.id;
    input.dataset.choice = q.id;                   // save/restore hook
    label.htmlFor = input.id;                      // explicit label
    label.append(
      input,
      el('span', String.fromCharCode(65 + index), 'choice__letter'), // A, B, C, D
      el('span', option.text, 'choice__text')
    );
    options.append(label);
  });
  fieldset.append(options);

  const actions = el('div', undefined, 'choice__actions');
  const check = el('button', '확인', 'primary'); check.type = 'button';
  check.dataset.checkChoice = q.id; check.setAttribute('aria-label', `${q.id} 답 확인`);
  const clear = el('button', '선택 해제', 'choice__clear'); clear.type = 'button';
  clear.dataset.clearChoice = q.id; clear.setAttribute('aria-label', `${q.id} 선택 해제`);
  const feedback = el('div', undefined, 'choice__feedback');
  feedback.dataset.choiceFeedback = q.id; feedback.id = `feedback-${q.id}`;
  feedback.hidden = true; feedback.setAttribute('role', 'status');
  feedback.setAttribute('aria-live', 'polite');
  check.setAttribute('aria-controls', feedback.id);
  actions.append(check, clear); fieldset.append(actions, feedback);
  item.append(fieldset);

  // Legacy text answers live here: collapsed, hidden until a saved answer exists.
  const legacy = el('details', undefined, 'legacy-answer');
  legacy.dataset.legacyAnswer = q.id;
  legacy.hidden = true;
  const legacyValue = el('p', undefined, 'legacy-answer__value');
  legacyValue.dataset.legacyAnswerValue = q.id;   // app.js sets this via textContent
  legacy.append(el('summary', '객관식 전환 전에 작성한 답안'), legacyValue);
  item.append(legacy);

  return item;
};
  function populateFirstQuestions(article) {
    const host = article.querySelector('#lesson-1-questions');
    if (!host || host.childElementCount) return;
    for (const q of bundle.firstQuestions) host.append(questionNode(q));
  }
  const link = (text, target) => {
    const node = el('a', text); node.href = `#${target}`; return node;
  };
  const available = new Set(['lesson-1', ...bundle.lessons.map(l => l.id)]);
  function buildLesson(lesson) {
    const number = Number(lesson.id.split('-')[1]);
    const article = el('article'); article.id = lesson.id; article.dataset.view = ''; article.hidden = true;
    const header = el('header');
    header.append(el('p', `LESSON ${String(number).padStart(2, '0')} · ${lesson.version}`, 'eyebrow'), el('h1', lesson.title), el('p', lesson.objective, 'lead'));
    article.append(header);
    const jumps = el('nav', undefined, 'jump-links'); jumps.setAttribute('aria-label', '이 차시 안에서 이동');
    jumps.append(link('짧은 복습', `${lesson.id}--solutions`), link('학습 목표', `${lesson.id}--goal`));
    lesson.sections.forEach((s, index) => jumps.append(link(s.title, `${lesson.id}--s${index + 1}`)));
    jumps.append(link('확인 문제', `${lesson.id}--quiz`)); article.append(jumps);
    // Keep the old section anchor so existing bookmarks still resolve.
    const recap = el('details', undefined, 'lesson-disclosure'); recap.id = `${lesson.id}--solutions`;
    const recapSummary = el('summary'); recapSummary.append(el('h2', '이어서 만들기 전, 짧은 복습'));
    const recall = el('ul'); lesson.recap.points.forEach(point => recall.append(el('li', point)));
    recap.append(recapSummary, recall, el('p', lesson.recap.bridge));
    article.append(recap);
    const goal = el('section');
    goal.id = `${lesson.id}--goal`;
    const goals = el('div', '', 'learning-goals');
    goals.append(el('h2', '이번 차시의 학습 목표'));
    const goalsList = el('dl', '');
    goalsList.append(el('dt', '출발점'), el('dd', lesson.start.replace(/^시작:\s*/, '')));
    goalsList.append(el('dt', '이번 차시의 도달점'), el('dd', lesson.finish.replace(/^완료:\s*/, '')));
    goals.append(goalsList);
    goal.append(goals);
    const prereq = el('div', '', 'learning-goals__prereq');
    prereq.append(el('span', '선수 지식: ', 'learning-goals__prereq-label'));
    prereq.append(el('span', lesson.prerequisites.join(' · ')));
    goal.append(prereq);
    article.append(goal);
    lesson.sections.forEach((section, index) => {
      const node = el('section'); node.id = `${lesson.id}--s${index + 1}`;
      node.append(el('p', `${String(index + 1).padStart(2, '0')} / 함께 만들기`, 'section-number'), el('h2', section.title));
      const prose = el('div', '', 'lesson-prose');
      // Only the local Markdown compiler produces this HTML; raw HTML is disabled.
      prose.innerHTML = section.html;
      const notes = Array.isArray(section.notes) ? section.notes : [];
      if (!notes.length) {
        // No notes: preserve the original simple prose behavior.
        node.append(prose);
      } else {
        const reading = el('div', '', 'lesson-reading');
        reading.append(prose);
      
        const aside = document.createElement('aside');
        aside.className = 'lesson-notes';
        aside.setAttribute('aria-label', '곁들여 읽기');
      
        notes.forEach((note) => {
          if (!note) return;
          const details = document.createElement('details');
          details.className = 'lesson-note';
      
          const summary = document.createElement('summary');
          if (note.kind) summary.append(el('span', note.kind, 'note-kind'));
          summary.append(el('span', note.title || '', 'note-title'));
          details.append(summary);
      
          const body = el('div', '', 'note-body');
          // Only the local Markdown compiler produces this HTML; raw HTML is disabled.
          body.innerHTML = note.html || '';
          details.append(body);
      
          aside.append(details);
        });
      
        reading.append(aside);
        node.append(reading);
      }
      for (const [codeIndex, code] of (section.codes || []).entries()) {
        const box = el('div', undefined, 'codebox');
        const bar = el('div', undefined, 'codebar');
        const ident = `${lesson.id}-code-${index}-${codeIndex}`;
        const button = el('button', '복사'); button.type = 'button'; button.dataset.copy = ident;
        bar.append(el('span', `강의 기준 코드 · ${code.label}`), button);
        const pre = el('pre'); const content = el('code', code.text); content.id = ident;
        content.className = `language-${code.language}`; pre.append(content); box.append(bar, pre);
        const lines = code.text.trimEnd().split('\n').length;
        if (code.file && (lines > 80 || code.collapsed === true)) {
          const full = el('details', undefined, 'checkpoint-file');
          full.append(el('summary', `${code.label} · ${lines}줄 펼쳐 보기`), box);
          node.append(full);
        } else node.append(box);
        if (window.hljs?.getLanguage(code.language)) window.hljs.highlightElement(content);
      }
      const ref = section.reference;
      if (ref) {
        const details = el('div', undefined, 'source-contract');
        const button = el('button', `${ref.path} · ${window.LEARNING_SITE?.mode === 'static' ? '배포본 구현 보기' : '현재 구현 보기'}`); button.type = 'button'; button.dataset.source = ref.path; button.dataset.needle = ref.symbol;
        details.append(button, el('p', `유지할 핵심: ${ref.invariant}`), el('p', `달라질 세부: ${ref.mayDiffer}`), el('p', `이 단계의 생략: ${ref.omitted}`));
        node.append(details);
      }
      article.append(node);
    });
    const practice = el('section'); practice.append(el('h2', '실습과 관찰 기록'));
    const steps = el('ol'); lesson.practice.steps.forEach(step => steps.append(el('li', step)));
    practice.append(steps, el('p', `기대 결과: ${lesson.practice.expected}`));
    practice.append(el('h3', '다르게 보인다면'));
    const failures = el('ul'); lesson.practice.failureChecks.forEach(check => failures.append(el('li', check)));
    practice.append(failures); article.append(practice);
    const quiz = el('details', undefined, 'lesson-disclosure'); quiz.id = `${lesson.id}--quiz`;
    const quizSummary = el('summary');
    quizSummary.append(el('h2', '확인 문제'));
    quiz.append(quizSummary, el('p', '각 문제에서 하나를 선택하고 확인을 누르세요. 정답과 해설이 바로 아래에 표시됩니다. 다른 선택으로 다시 풀 수도 있습니다.'));
    for (const q of lesson.questions) quiz.append(questionNode(q));
    const actions = el('div', undefined, 'actions');
    const copy = el('button', '이 차시 답안 복사'); copy.type = 'button'; copy.dataset.copyAnswers = lesson.id;
    const label = el('label', undefined, 'completion'); const complete = el('input'); complete.type = 'checkbox'; complete.dataset.complete = lesson.id;
    label.append(complete, document.createTextNode(` ${number}차시 학습 완료로 표시`)); actions.append(copy, label); quiz.append(actions);
    quiz.append(el('p', '선택과 확인 결과는 이 브라우저에 저장됩니다. 학습 완료 표시는 직접 관리하며 정답 개수로 자동 결정하지 않습니다.', 'reference'));
    article.append(quiz);
    const end = el('nav', undefined, 'lesson-next');
    end.append(link(`← ${number - 1}차시로 이동`, `lesson-${number - 1}`));
    if (available.has(`lesson-${number + 1}`)) end.append(link(`${number + 1}차시로 이동 →`, `lesson-${number + 1}`));
    else end.append(link('다음 학습 주제 보기 →', 'roadmap'));
    article.append(end);
    return article;
  }
  for (const lesson of bundle.lessons) {
    const number = Number(lesson.id.split('-')[1]);
    const entry = link(`${String(number).padStart(2, '0')} · ${lesson.shortTitle}`, lesson.id); entry.dataset.page = lesson.id;
    const status = el('span', '', 'nav-status'); status.dataset.lessonStatus = lesson.id; status.hidden = true; entry.append(status); nav.append(entry);
  }
  const count = available.size;
  document.querySelectorAll('[data-course-status]').forEach(node => { node.textContent = `현재 ${count}개 차시를 읽을 수 있습니다. 나머지는 집필 중입니다. 원문 전체 통합과 강의 집필 완료는 구분합니다.`; });
  const coverage = el('section'); coverage.id = 'coverage-progress';
  const rows = bundle.coverage.sections;
  coverage.append(el('h2', '원문 전체를 어떻게 따라가고 있나?'), el('p', `${rows.length}개 원문 절을 추적합니다. ${rows.filter(r => r.state === 'covered').length}개 절은 전체 내용 대응 검토를 마쳤고, ${rows.filter(r => r.state === 'partial').length}개 절은 일부가 강의에 연결됐습니다. 차시를 집필했다는 사실만으로 원문 전체를 다뤘다고 표시하지 않습니다.`));
  for (let part = 0; part <= 18; part++) {
    const details = el('details'); details.append(el('summary', `Part ${part} · 원문 절과 집필 상태`));
    const list = el('ul');
    for (const row of rows.filter(r => r.part === part)) {
      const item = el('li');
      const button = el('button', row.title, 'text-button'); button.type = 'button'; button.dataset.doc = `part${part}`;
      if (row.title !== '표제와 도입') button.dataset.docTitle = row.title;
      item.append(button, document.createTextNode(` — ${{unassigned:'대응 검토 전',partial:'일부 연결',covered:'대응 검토 완료','needs-review':'원문 변경·재검토 필요'}[row.state] || row.state}`));
      for (const lesson of row.lessons) if (available.has(lesson)) item.append(document.createTextNode(' · '), link(`${lesson.split('-')[1]}차시로 이동`, lesson));
      if (row.routingState === 'draft') {
        item.append(el('p', `제목 기반 집필 후보: ${row.candidateUnits.map(id => `${Number(id.split('-')[1])}차시`).join(', ')} · 본문 대조 전`, 'reference'));
      }
      list.append(item);
    }
    details.append(list); coverage.append(details);
  }
  document.getElementById('roadmap').append(coverage);

  const lessonMap = new Map(bundle.lessons.map(lesson => [lesson.id, lesson]));
  let mountedPage = null;
  let mountedNode = null;
  function unmount() {
    if (mountedNode && mountedNode.isConnected) mountedNode.remove();
    mountedNode = null; mountedPage = null;
  }
  function mount(page) {
    if (mountedPage === page && mountedNode && mountedNode.isConnected) return;
    unmount();
    let article = null;
    if (page === firstLessonId) {
      if (firstTemplate) {
        article = firstTemplate.cloneNode(true);
        article.hidden = true;
        populateFirstQuestions(article);
      }
    } else if (lessonMap.has(page)) {
      article = buildLesson(lessonMap.get(page));
    }
    if (!article) return;
    main.insertBefore(article, document.querySelector('.reading-shortcuts'));
    mountedPage = page; mountedNode = article;
    document.dispatchEvent(new CustomEvent('learning:mounted', { detail: { page } }));
  }
  // Anchor metadata stays available even when its article is not mounted.
  const firstAnchors = new Map([...firstTemplate.querySelectorAll('[id]')].map(node => [
    node.id, node.querySelector('h2, h3, summary')?.textContent.trim() || '본문'
  ]));
  const anchorLabels = { goal: '학습 목표', solutions: '짧은 복습', quiz: '확인 문제' };
  function anchorLabel(hash) {
    if (typeof hash !== 'string' || hash.split('--').length > 2 || !/^lesson-\d+(--[a-z0-9-]+)?$/.test(hash)) return null;
    const [page, anchor] = hash.split('--');
    if (!has(page)) return null;
    if (!anchor) return title(page);
    if (page === firstLessonId) return firstAnchors.get(hash) || null;
    if (Object.hasOwn(anchorLabels, anchor)) return anchorLabels[anchor];
    const match = /^s([1-9]\d*)$/.exec(anchor);
    return match ? lessonMap.get(page).sections[Number(match[1]) - 1]?.title || null : null;
  }
  function title(page) {
    if (page === firstLessonId) return firstTemplate?.querySelector('h1')?.textContent.replace(/\s+/g, ' ').trim() || '1차시';
    return lessonMap.get(page)?.title || page;
  }
  function has(page) {
    return page === firstLessonId || lessonMap.has(page);
  }
  window.LEARNING_COURSE = { firstLesson: firstLessonId, mount, unmount, has, title, anchorLabel };
})();
