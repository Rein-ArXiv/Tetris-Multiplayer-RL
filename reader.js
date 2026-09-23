(() => {
  'use strict';
  const library = window.LEARNING_LIBRARY;
  if (!library) return;
  const reader = document.getElementById('reference-reader');
  const file = document.getElementById('reader-file');
  const filter = document.getElementById('reader-filter');
  const headings = document.getElementById('reader-heading');
  const body = document.getElementById('reader-body');
  const scroll = document.getElementById('reader-scroll');
  const provenance = document.getElementById('reader-provenance');
  let mode = 'doc';
  let selected = { doc: 'part0', code: 'CMakeLists.txt' };
  let request = 0;
  let codeText = '';
  let opener;
  let lastNeedle = '';
  function labelStaticSources() {
    if (window.LEARNING_SITE?.mode !== 'static') return;
    document.getElementById('reader-refresh').textContent = '배포본 코드 다시 보기';
    document.getElementById('reader-code-mode').textContent = '배포본 코드';
    for (const button of document.querySelectorAll('button[data-source]')) {
      button.textContent = button.textContent.replace(/^현재 /, '배포본의 ');
    }
  }
  labelStaticSources();
  document.addEventListener('learning:mounted', labelStaticSources);
  // Diagrams use a light canvas so saved theme changes cannot hide SVG labels/edges.
  if (window.mermaid) window.mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: 'default', flowchart: { htmlLabels: false }, suppressErrorRendering: true });
  const languageFor = path => /CMakeLists/.test(path) ? 'cmake' : path.endsWith('.py') ? 'python' : path.endsWith('.sh') ? 'bash' : path.endsWith('.ps1') ? 'powershell' : 'cpp';
  if (library.coursePlan) {
    const plan = document.createElement('section');
    const title = document.createElement('h2'); title.textContent = '세부 강의 편성안';
    const intro = document.createElement('p'); intro.textContent = '176개 구현 주제와 마지막 해설을 순서대로 편성했습니다. 링크가 있는 차시만 본문이 제공되며, 나머지는 집필 예정입니다. CS 깊이를 위해 필요한 경우 차시를 더 나눕니다.';
    plan.append(title, intro);
    for (const module of library.coursePlan.modules) {
      const details = document.createElement('details');
      const summary = document.createElement('summary'); summary.textContent = `${module.title} · ${module.units.length}개 주제`;
      const list = document.createElement('ol');
      list.start = Number(module.units[0].id.split('-')[1]);
      for (const unit of module.units) {
        const li = document.createElement('li');
        // Availability comes from reviewed data, not the currently mounted article.
        if (window.LEARNING_COURSE?.has(unit.lessonId)) {
          const link = document.createElement('a'); link.href = `#${unit.lessonId}`; link.textContent = unit.title; li.append(link);
        } else li.textContent = `${unit.title} · 집필 예정`;
        list.append(li);
      }
      details.append(summary, list); plan.append(details);
    }
    document.getElementById('roadmap').insertBefore(plan, document.getElementById('coverage-progress'));
  }
  function highlight(container) {
    if (!window.hljs) return;
    for (const code of container.querySelectorAll('pre:not(.mermaid-source) > code')) {
      const language = code.className.match(/language-([\w+-]+)/)?.[1];
      if (language && window.hljs.getLanguage(language)) window.hljs.highlightElement(code);
    }
  }

  const option = (value, label) => {
    const el = document.createElement('option'); el.value = value; el.textContent = label; return el;
  };
  const dataset = () => mode === 'doc' ? library.documents : library.sources;
  function populate() {
    const query = filter.value.toLocaleLowerCase();
    file.replaceChildren();
    for (const [id, entry] of Object.entries(dataset())) {
      const label = mode === 'doc' ? entry.title : id;
      if (!`${id} ${label}`.toLocaleLowerCase().includes(query)) continue;
      file.append(option(id, label));
    }
    if ([...file.options].some(item => item.value === selected[mode])) file.value = selected[mode];
    file.disabled = !file.options.length;
    document.getElementById('reader-heading-field').hidden = mode !== 'doc';
    document.getElementById('reader-refresh').hidden = mode !== 'code';
    document.getElementById('reader-copy').hidden = mode !== 'code';
    document.getElementById('reader-doc-mode').setAttribute('aria-pressed', String(mode === 'doc'));
    document.getElementById('reader-code-mode').setAttribute('aria-pressed', String(mode === 'code'));
  }
  function jump(id) {
    const target = [...body.querySelectorAll('[id]')].find(item => item.id === id);
    if (target) scroll.scrollTo({ top: target.getBoundingClientRect().top - scroll.getBoundingClientRect().top + scroll.scrollTop - 16, behavior: 'instant' });
  }
  async function render({ needle = '', anchor = '', headingTitle = '' } = {}) {
    const generation = ++request;
    const id = file.value;
    codeText = '';
    document.getElementById('reader-copy').disabled = true;
    body.replaceChildren(); scroll.scrollTop = 0;
    if (!id) { provenance.textContent = '일치하는 파일이 없습니다.'; return; }
    selected[mode] = id;
    if (mode === 'doc') {
      const doc = library.documents[id];
      // HTML was rendered locally from repository Markdown with raw HTML disabled.
      body.innerHTML = doc.html;
      highlight(body);
      body.className = 'reference-document';
      provenance.textContent = `상세 참고 원문 · ${doc.path} · 변환본 ${doc.sha256.slice(0, 10)} · 강의 작성 완료 표시가 아닙니다`;
      headings.replaceChildren(...doc.headings.map(item => option(item.id, `${'　'.repeat(Math.max(0, item.level - 1))}${item.title}`)));
      const found = doc.headings.find(item => (anchor && item.slug === anchor) || (headingTitle && item.title.startsWith(headingTitle)));
      if (found) { headings.value = found.id; jump(found.id); }
      if (window.mermaid) {
        for (const diagram of body.querySelectorAll('[data-diagram]')) {
          if (generation !== request) break;
          const source = diagram.querySelector('code').textContent;
          try {
            const result = await window.mermaid.render(`diagram-${generation}-${Math.random().toString(36).slice(2)}`, source);
            if (generation !== request) break;
            diagram.querySelector('.diagram-drawing').innerHTML = result.svg;
          } catch (_) { diagram.querySelector('details').open = true; }
        }
      } else {
        body.querySelectorAll('[data-diagram] details').forEach(item => { item.open = true; });
      }
      if (generation === request && found && headings.value === found.id) jump(found.id);
      return;
    }
    lastNeedle = needle;
    const staticSite = window.LEARNING_SITE?.mode === 'static';
    provenance.textContent = staticSite ? '배포본의 소스 레퍼런스를 읽는 중…' : '현재 작업 폴더에서 읽는 중…';
    const fallback = library.sources[id];
    let source = fallback;
    let live = false;
    try {
      if (staticSite || !['http:', 'https:'].includes(location.protocol)) throw new Error('static');
      const response = await fetch(`/__learn/source?path=${encodeURIComponent(id)}`, { cache: 'no-store', signal: AbortSignal.timeout(4000) });
      if (!response.ok || !response.headers.get('content-type')?.includes('application/json')) throw new Error('static');
      source = await response.json();
      if (source.kind !== 'working-tree' || typeof source.text !== 'string') throw new Error('format');
      live = true;
    } catch (_) { source = fallback; }
    if (generation !== request) return;
    codeText = source.text;
    document.getElementById('reader-copy').disabled = false;
    const rows = codeText.split('\n');
    const index = needle ? rows.findIndex(line => line.includes(needle)) : -1;
    const lines = document.createElement('pre'); lines.className = 'source-lines';
    rows.forEach((text, position) => {
      const row = document.createElement('span'); row.className = 'source-line'; row.id = `source-line-${position + 1}`;
      const number = document.createElement('span'); number.className = 'line-number'; number.textContent = position + 1; number.setAttribute('aria-hidden', 'true');
      const content = document.createElement('code');
      const language = languageFor(id);
      if (window.hljs?.getLanguage(language)) content.innerHTML = window.hljs.highlight(text || ' ', { language, ignoreIllegals: true }).value;
      else content.textContent = text || ' ';
      row.append(number, content);
      if (index >= 0 && position >= index && position < index + 12) row.classList.add('source-focus');
      lines.append(row);
    });
    body.className = 'reference-source'; body.replaceChildren(lines);
    const status = live ? `현재 작업 폴더 · ${new Date().toLocaleTimeString('ko-KR')} 확인`
      : staticSite ? `배포본 ${window.LEARNING_SITE.release}의 소스 스냅샷 · 자동 실시간 갱신 아님`
      : '보관 스냅샷 · 실시간 파일을 읽지 못했습니다';
    provenance.textContent = `${status} · ${source.sha256.slice(0, 10)} · 강의 기준 코드와 세부가 다를 수 있습니다.${needle && index < 0 ? ' 요청한 심볼을 찾지 못했습니다. 파일 구조 변경을 확인하세요.' : ''}`;
    if (index >= 0) jump(`source-line-${index + 1}`);
  }
  // Preserve a visible DOM anchor across the synchronous column-width change.
  // Keeping scrollY alone would keep pixels, but lose the paragraph after reflow.
  function anchorPosition(element) {
    if (window.innerWidth < 850 || !element?.isConnected || !element.closest('main')) return null;
    const top = element.getBoundingClientRect().top;
    return top >= 0 && top < window.innerHeight ? { element, top } : null;
  }
  function restorePosition(anchor) {
    if (anchor?.element.isConnected) {
      const delta = anchor.element.getBoundingClientRect().top - anchor.top;
      window.scrollBy({ top: delta, left: 0, behavior: 'instant' });
    }
  }
  function open(nextMode, id, opts = {}) {
    const entering = !reader.contains(document.activeElement);
    if (entering) opener = opts.trigger || document.activeElement;
    const anchor = anchorPosition(opener);
    mode = nextMode;
    if (id && dataset()[id]) selected[mode] = id;
    filter.value = '';
    reader.hidden = false;
    document.querySelector('.layout').classList.add('reader-open');
    populate(); render(opts);
    if (entering) document.getElementById('reader-close').focus({ preventScroll: true });
    restorePosition(anchor);
    if (window.innerWidth < 850) reader.scrollIntoView({ behavior: 'instant' });
  }
  function closeReader() {
    const anchor = anchorPosition(opener);
    ++request; reader.hidden = true; document.querySelector('.layout').classList.remove('reader-open');
    const returnTarget = opener?.isConnected ? opener : document.getElementById('content');
    returnTarget.focus({ preventScroll: true });
    restorePosition(anchor);
    if (window.innerWidth < 850 && opener?.isConnected) opener.scrollIntoView({ block: 'center', behavior: 'instant' });
  }
  document.getElementById('reader-close').addEventListener('click', closeReader);
  reader.addEventListener('keydown', event => {
    if (event.key === 'Escape') { event.preventDefault(); closeReader(); }
  });
  document.getElementById('reader-doc-mode').addEventListener('click', () => open('doc'));
  document.getElementById('reader-code-mode').addEventListener('click', () => open('code'));
  filter.addEventListener('input', () => { populate(); render(); });
  file.addEventListener('change', () => render());
  headings.addEventListener('change', () => jump(headings.value));
  document.getElementById('reader-refresh').addEventListener('click', () => render({ needle: lastNeedle }));
  document.getElementById('reader-copy').addEventListener('click', async () => {
    try { await navigator.clipboard.writeText(codeText); provenance.textContent += ' · 복사했습니다.'; }
    catch (_) { provenance.textContent += ' · 자동 복사 불가: 코드를 선택해 복사하세요.'; }
  });
  document.addEventListener('click', async event => {
    const el = event.target.closest('[data-doc],[data-source],[data-open-library],[data-copy-block],[data-unbundled]');
    if (!el) return;
    event.preventDefault();
    if (el.hasAttribute('data-source')) open('code', el.dataset.source, { needle: el.dataset.needle || '', trigger: el });
    else if (el.hasAttribute('data-doc')) open('doc', el.dataset.doc, { anchor: el.dataset.docAnchor || '', headingTitle: el.dataset.docTitle || '', trigger: el });
    else if (el.hasAttribute('data-open-library')) open(mode);
    else if (el.hasAttribute('data-unbundled')) provenance.textContent = `이 자료는 아직 뷰어에 포함되지 않았습니다: ${el.dataset.unbundled}`;
    else {
      try { await navigator.clipboard.writeText(el.closest('.reference-code').querySelector('code').textContent); el.textContent = '복사됨'; }
      catch (_) { el.textContent = '선택해서 복사하세요'; }
    }
  });
})();
