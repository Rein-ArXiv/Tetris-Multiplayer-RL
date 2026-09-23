(() => {
  'use strict';
  const key = 'tetris-learning-v1';
  const storageStatus = document.getElementById('storage-status');
  const actionStatus = document.getElementById('action-status');
  const course = window.LEARNING_COURSE;
  const questionIndex = window.LEARNING_QUESTION_INDEX || new Map();
  let state = { answers: {}, choices: {}, confirmedChoices: {}, complete: false, completedLessons: {} };
  let storageWorks = true;
  let noticeTimer;
  let savedStatusTimer;

  try {
    const saved = JSON.parse(localStorage.getItem(key) || 'null');
    if (saved && typeof saved === 'object') {
      state.complete = saved.complete === true;
      for (const [id, value] of Object.entries(saved.answers || {})) {
        // Preserve earlier and future lesson answers, even if not rendered today.
        if (/^\d+-\d+$/.test(id) && typeof value === 'string') state.answers[id] = value;
      }
      for (const [id, value] of Object.entries(saved.completedLessons || {})) {
        if (/^lesson-\d+$/.test(id)) state.completedLessons[id] = value === true;
      }
      for (const kind of ['choices', 'confirmedChoices']) {
        for (const [id, value] of Object.entries(saved[kind] || {})) {
          // Keep future lesson records too; option IDs are stable after review.
          if (/^\d+-\d+$/.test(id) && /^[a-d]$/.test(value)) state[kind][id] = value;
        }
      }
      if (!Object.hasOwn(state.completedLessons, 'lesson-1')) state.completedLessons['lesson-1'] = state.complete;
    }
    // Check write access without overwriting existing answers.
    localStorage.setItem(key, JSON.stringify(state));
  } catch (_) { storageWorks = false; }

  function storageLabel(saved = false) {
    storageStatus.textContent = storageWorks
      ? (saved ? '이 브라우저에 저장됨' : '답안 · 이 브라우저에만 저장')
      : '저장 불가 · 답안을 복사해 보관하세요';
  }
  function persist() {
    try { localStorage.setItem(key, JSON.stringify(state)); storageWorks = true; }
    catch (_) { storageWorks = false; }
    // Avoid repeatedly interrupting screen readers while an answer is being typed.
    clearTimeout(savedStatusTimer);
    if (!storageWorks) storageLabel();
    else savedStatusTimer = setTimeout(() => storageLabel(true), 700);
  }
  function notice(message) {
    clearTimeout(noticeTimer);
    actionStatus.textContent = message;
    noticeTimer = setTimeout(() => { actionStatus.textContent = ''; }, 5500);
  }
  function completionLabel() {
    const lessonStatus = document.getElementById('lesson-status');
    const lessonDone = state.completedLessons['lesson-1'];
    lessonStatus.textContent = lessonDone ? '완료 표시됨' : '';
    lessonStatus.hidden = !lessonDone;
    for (const label of document.querySelectorAll('[data-lesson-status]')) {
      const done = state.completedLessons[label.dataset.lessonStatus];
      label.textContent = done ? '완료 표시됨' : '';
      label.hidden = !done;
    }
  }
  function savedAnswerLabels() {
    // Keep written answers from before the format change; never reinterpret them
    // as a letter or overwrite them when a new choice is selected.
    for (const panel of document.querySelectorAll('[data-legacy-answer]')) {
      const value = state.answers[panel.dataset.legacyAnswer] || '';
      panel.hidden = !value.trim();
      panel.querySelector('[data-legacy-answer-value]').textContent = value;
    }
  }
  function selectionText(id) {
    const option = questionIndex.get(id)?.options.find(o => o.id === state.choices[id]);
    return option ? `${option.id.toUpperCase()} · ${option.text}` : '(선택하지 않음)';
  }
  function feedback(id, missing = false) {
    const target = document.querySelector(`[data-choice-feedback="${id}"]`);
    const q = questionIndex.get(id);
    target.replaceChildren();
    if (missing) {
      target.textContent = '먼저 답을 하나 선택한 뒤 확인을 눌러 주세요.';
      target.hidden = false;
      return;
    }
    const selected = state.choices[id];
    if (!selected || state.confirmedChoices[id] !== selected) { target.hidden = true; return; }
    const correct = q.options.find(option => option.id === q.correctOptionId);
    const line = (tag, text) => { const node = document.createElement(tag); node.textContent = text; return node; };
    target.append(line('strong', selected === q.correctOptionId ? '맞았습니다.' : '정답이 아닙니다. 해설을 확인해 보세요.'),
      line('p', `내 선택 · ${selectionText(id)}`),
      line('p', `정답 · ${correct.id.toUpperCase()} · ${correct.text}`),
      line('p', q.explanation));
    target.hidden = false;
  }
  function syncLesson() {
    // Reapply saved state to whatever DOM the current lesson mounted.
    for (const radio of document.querySelectorAll('[data-choice]')) {
      radio.checked = state.choices[radio.dataset.choice] === radio.value;
    }
    for (const button of document.querySelectorAll('[data-check-choice]')) feedback(button.dataset.checkChoice);
    for (const field of document.querySelectorAll('[data-answer]')) field.value = state.answers[field.dataset.answer] || '';
    for (const checkbox of document.querySelectorAll('[data-complete]')) checkbox.checked = state.completedLessons[checkbox.dataset.complete] === true;
    completionLabel();
    savedAnswerLabels();
  }
  document.addEventListener('change', event => {
    const radio = event.target.closest('[data-choice]');
    if (radio) {
      if (!radio.checked) return;
      const id = radio.dataset.choice;
      state.choices[id] = radio.value;
      delete state.confirmedChoices[id];
      feedback(id);
      persist();
      return;
    }
    const checkbox = event.target.closest('[data-complete]');
    if (checkbox) {
      state.completedLessons[checkbox.dataset.complete] = checkbox.checked;
      state.complete = state.completedLessons['lesson-1'] === true;
      completionLabel();
      persist();
    }
  });
  document.addEventListener('input', event => {
    const field = event.target.closest('[data-answer]');
    if (!field) return;
    state.answers[field.dataset.answer] = field.value;
    persist();
    savedAnswerLabels();
  });
  document.addEventListener('click', event => {
    const check = event.target.closest('[data-check-choice]');
    if (check) {
      const id = check.dataset.checkChoice;
      if (!state.choices[id]) {
        feedback(id, true);
        document.querySelector(`[data-choice="${id}"]`)?.focus();
        return;
      }
      state.confirmedChoices[id] = state.choices[id];
      feedback(id);
      persist();
      return;
    }
    const clear = event.target.closest('[data-clear-choice]');
    if (clear) {
      const id = clear.dataset.clearChoice;
      delete state.choices[id]; delete state.confirmedChoices[id];
      for (const radio of document.querySelectorAll(`[data-choice="${id}"]`)) radio.checked = false;
      feedback(id);
      persist();
      return;
    }
    const copyButton = event.target.closest('[data-copy]');
    if (copyButton) {
      copy(document.getElementById(copyButton.dataset.copy)?.textContent || '');
      return;
    }
    const answersButton = event.target.closest('[data-copy-answers]');
    if (answersButton) {
      const lesson = document.getElementById(answersButton.dataset.copyAnswers);
      const text = [...lesson.querySelectorAll('[data-question]')].map(item => {
        const id = item.dataset.question;
        const legacy = state.answers[id]?.trim();
        return `${id}\n${selectionText(id)}${legacy ? `\n이전 서술 답안: ${legacy}` : ''}`;
      }).join('\n\n');
      copy(`테트리스 동행 학습 · ${lesson.id.split('-')[1]}차시 답안\n\n${text}`);
      return;
    }
    const gpuButton = event.target.closest('[data-gpu]');
    if (gpuButton) {
      const entry = gpu[Number(gpuButton.dataset.gpu)];
      ['gpu-owner', 'gpu-title', 'gpu-description', 'gpu-source', 'gpu-question'].forEach((id, index) => {
        document.getElementById(id).textContent = entry[index];
      });
      for (const item of document.querySelectorAll('[data-gpu]')) item.setAttribute('aria-pressed', String(item === gpuButton));
    }
  });
  document.addEventListener('learning:mounted', () => syncLesson());
  storageLabel();
  syncLesson();

  function mountPage(page) {
    // Only the currently selected view is visible; lessons are mounted on demand.
    for (const view of document.querySelectorAll('[data-view]')) view.hidden = true;
    if (course?.has(page)) course.mount(page);
    else course?.unmount();
    const target = document.getElementById(page);
    if (target) target.hidden = false;
    return target;
  }
  function resolvePage(requestedPage) {
    if (course?.has(requestedPage)) return requestedPage;
    const candidate = document.getElementById(requestedPage);
    if (candidate && candidate.hasAttribute('data-view')) return requestedPage;
    return 'lesson-1';
  }
  function route(initial = false) {
    const hash = location.hash.slice(1) || 'lesson-1';
    if (hash === 'content' && !initial) {
      document.getElementById('content').focus({ preventScroll: true });
      window.scrollTo({ top: 0, behavior: 'instant' });
      return;
    }
    if (hash === 'course-sidebar' && !initial) {
      return;
    }
    const requestedPage = hash.split('--')[0];
    const page = resolvePage(requestedPage);
    const target = mountPage(page);
    for (const link of document.querySelectorAll('[data-page]')) {
      if (link.dataset.page === page) link.setAttribute('aria-current', 'page');
      else link.removeAttribute('aria-current');
    }
    if (!target) return;
    const heading = target.querySelector('h1').cloneNode(true);
    heading.querySelectorAll('br').forEach(br => br.replaceWith(' '));
    const title = heading.textContent.replace(/\s+/g, ' ');
    document.title = `${title} — 프로젝트 동행`;
    if (hash.includes('--') && document.getElementById(hash)) {
      const anchor = document.getElementById(hash);
      const disclosure = anchor.closest('details.lesson-disclosure');
      if (disclosure) disclosure.open = true;
      const focusTarget = anchor.matches('details') ? anchor.querySelector('summary') : anchor;
      if (!focusTarget.matches('summary')) focusTarget.tabIndex = -1;
      if (!initial) focusTarget.focus({ preventScroll: true });
      anchor.scrollIntoView();
    } else if (!initial) {
      document.getElementById('content').focus({ preventScroll: true });
      window.scrollTo({ top: 0, behavior: 'instant' });
    }
  }
  window.addEventListener('hashchange', () => route());
  // A closed panel must also reopen when its anchor is already the current hash.
  document.addEventListener('click', event => {
    const link = event.target.closest('a[href^="#"]');
    if (!link || event.button !== 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
    if (link.hash === location.hash && (link.hash.includes('--') || link.hash === '#content' || link.hasAttribute('data-page'))) {
      event.preventDefault();
      route();
    }
  });
  route(true);

  async function copy(text) {
    try {
      if (!navigator.clipboard?.writeText) throw new Error('Clipboard unavailable');
      await navigator.clipboard.writeText(text);
      notice('복사했습니다. 원하는 곳에 붙여 넣으세요.');
    } catch (_) { notice('이 환경에서는 자동 복사가 제한됩니다. 내용을 선택해 복사해 주세요.'); }
  }

  const gpu = [
    ['CPU · 게임의 표현 코드', '그릴 사각형을 요청합니다', 'draw_rect는 위치·크기·색을 받습니다. 게임 규칙은 화면을 모르고, 표현 코드가 게임 상태를 읽어 그릴 것을 정합니다.', 'renderer/renderer.cpp · draw_rect', '게임 보드의 행·열은 화면의 x·y로 어떻게 바뀔까요? 규칙과 그림을 왜 분리할까요?'],
    ['CPU · 메모리', '사각형을 정점 여섯 개로 만듭니다', 'glb_rect는 삼각형 두 개를 표현할 정점을 s_verts에 모읍니다. 현재 구현은 정점당 14개의 float를 사용합니다. 정점 목록에 추가하는 것과 GPU 작업 완료는 다른 일입니다.', 'renderer/renderer.cpp · glb_rect / push_vertex', '위치·UV·색 등을 한 배열에 넣으면 다음 정점까지 몇 바이트일까요? stride와 offset은 무엇을 가리킬까요?'],
    ['CPU → OpenGL 드라이버', '버퍼 데이터를 지정하고 그리기를 제출합니다', 'glb_flush는 gl_BufferData로 버퍼의 데이터 저장소를 만들고 초기화한 뒤 gl_DrawArrays를 호출합니다. 실제 메모리 배치·전송 시점은 구현이 관리합니다. 호출이 반환됐다고 GPU 작업이 끝났다는 뜻은 아닙니다.', 'renderer/renderer.cpp · glb_flush', 'VAO는 어떤 해석 정보를 보관할까요? gl_DrawArrays가 받는 count는 바이트 수일까요, 정점 수일까요?'],
    ['그래픽 파이프라인 · 일반적인 하드웨어 가속 경로', '정점에서 프래그먼트와 색으로 이어집니다', '정점 셰이더는 좌표를 변환합니다. 이 코드의 gl_Position.w는 1이라 원근 나눗셈 뒤 NDC의 xyz가 그대로입니다. 래스터화가 프래그먼트를 만들고, 프래그먼트 셰이더가 텍스처·색을 계산하며 블렌딩이 결과를 합성합니다.', 'renderer/gl_shaders.h · kQuadVert / kQuadFrag', '래스터화와 셰이더는 무엇이 다를까요? 화면 좌상단 원점은 왜 y 변환이 필요할까요?'],
    ['플랫폼 · 윈도 시스템', '프레임을 표시하도록 요청합니다', 'renderer_end는 남은 배치를 비우고 platform_present를 호출합니다. SDL 경로에서는 SDL_GL_SwapWindow를 사용합니다. 실제 표시에는 윈도 시스템·컴포지터·디스플레이 경로도 관여하므로 호출과 모니터 표시를 같은 순간으로 보지 않습니다.', 'renderer/renderer.cpp · renderer_end → platform/sdl.cpp · platform_present', '앞·뒤 버퍼는 왜 필요할까요? VSync와 게임의 고정 스텝은 왜 따로 생각해야 할까요?']
  ];
})();
