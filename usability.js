/* Reading preferences are independent of answers and never sent to a server. */
(() => {
  'use strict';
  const storageKey = 'tetris-learning-reading-v1';
  const root = document.documentElement;
  const size = document.getElementById('reading-size');
  const theme = document.getElementById('reading-theme');
  const wrap = document.getElementById('reading-wrap');
  const status = document.getElementById('reading-settings-status');
  const resume = document.getElementById('resume-reading');
  const prefs = { size: 'normal', theme: 'system', wrap: false, location: '', previousLocation: '' };
  const validLocation = hash => typeof hash === 'string' && /^lesson-\d+(--[a-z0-9-]+)?$/.test(hash)
    && Boolean(window.LEARNING_COURSE?.anchorLabel(hash));
  let currentHash = '';
  let currentLesson = '';
  try {
    const saved = JSON.parse(localStorage.getItem(storageKey) || 'null');
    if (saved && typeof saved === 'object') {
      if (['normal', 'large', 'larger'].includes(saved.size)) prefs.size = saved.size;
      if (['system', 'light', 'dark'].includes(saved.theme)) prefs.theme = saved.theme;
      prefs.wrap = saved.wrap === true;
      if (validLocation(saved.location)) prefs.location = saved.location;
      if (validLocation(saved.previousLocation)) prefs.previousLocation = saved.previousLocation;
    }
  } catch (_) { status.textContent = '읽기 설정을 불러올 수 없습니다. 현재 화면에서는 변경할 수 있습니다.'; }
  function save() {
    try { localStorage.setItem(storageKey, JSON.stringify(prefs)); }
    catch (_) { status.textContent = '읽기 설정 저장 불가 · 이번에 연 화면에만 적용합니다.'; }
  }
  function apply() {
    root.dataset.readingSize = prefs.size;
    root.dataset.theme = prefs.theme;
    root.classList.toggle('wrap-code', prefs.wrap);
    size.value = prefs.size; theme.value = prefs.theme; wrap.checked = prefs.wrap;
  }
  // app.js handles routing first; use its visible view rather than duplicate its fallback rules.
  function resolveHash(hash) {
    const page = document.querySelector('article[data-view]:not([hidden])')?.id || 'lesson-1';
    if (validLocation(hash) && hash.split('--')[0] === page) return hash;
    if ((hash === 'content' || hash === 'course-sidebar') && currentHash.split('--')[0] === page) return currentHash;
    return page;
  }
  function labelFor(hash) {
    const [page, anchor] = hash.split('--');
    const coursePart = window.LEARNING_COURSE;
    const label = anchor ? coursePart?.anchorLabel(hash) : coursePart?.title(page);
    return `${page.split('-')[1]}차시 · ${label || '본문'}`;
  }
  function showResume() {
    // On a lesson page show the previous lesson; on guide/roadmap/graphics show the last lesson read.
    let target = currentLesson ? prefs.previousLocation : prefs.location;
    if (!validLocation(target) || target === currentHash) target = '';
    resume.hidden = !target;
    document.getElementById('resume-empty').hidden = Boolean(target);
    if (!target) return;
    resume.href = `#${target}`;
    resume.textContent = `이전 읽던 위치: ${labelFor(target)}`;
  }
  // Remember a newly opened lesson; the lesson saved before it becomes the previous one.
  function visit(lesson) {
    if (!lesson || lesson === prefs.location) return;
    prefs.previousLocation = prefs.location;
    prefs.location = lesson;
    save();
  }
  for (const control of [size, theme, wrap]) control.addEventListener('change', () => {
    prefs.size = size.value; prefs.theme = theme.value; prefs.wrap = wrap.checked;
    apply(); save();
  });
  // Honour an explicit deep link as-is: resolve the real start page before recording it.
  currentHash = resolveHash(location.hash.slice(1));
  currentLesson = validLocation(currentHash) ? currentHash : '';
  visit(currentLesson);
  apply();
  document.getElementById('resume-empty').textContent = '다른 차시나 절을 읽으면 이전 위치로 돌아갈 수 있습니다.';
  showResume();
  window.addEventListener('hashchange', () => {
    const hash = location.hash.slice(1);
    currentHash = resolveHash(hash);
    currentLesson = validLocation(currentHash) ? currentHash : '';
    visit(currentLesson);
    showResume();
  });

  const menu = document.getElementById('course-menu');
  const toggle = document.getElementById('course-menu-toggle');
  const mobile = matchMedia('(max-width:620px)');
  function setMenu(open) {
    menu.hidden = !open;
    toggle.setAttribute('aria-expanded', String(open));
    toggle.textContent = open ? '학습 목차 접기' : '학습 목차 펼치기';
  }
  function responsiveMenu() {
    toggle.hidden = !mobile.matches;
    setMenu(!mobile.matches);
  }
  toggle.addEventListener('click', () => setMenu(menu.hidden));
  mobile.addEventListener('change', responsiveMenu);
  responsiveMenu();
  menu.addEventListener('click', event => {
    if (mobile.matches && event.target.closest('a[data-page]')) setMenu(false);
  });
  document.getElementById('jump-course-menu').addEventListener('click', event => {
    event.preventDefault();
    // The reader hides the sidebar on desktop; restore the lecture layout first.
    if (!document.getElementById('reference-reader').hidden) document.getElementById('reader-close').click();
    setMenu(true);
    document.getElementById('lesson-search').focus({ preventScroll: true });
    document.getElementById('course-sidebar').scrollIntoView({ behavior: 'instant' });
  });
  const search = document.getElementById('lesson-search');
  const lessonLinks = [...menu.querySelectorAll('a[data-page^="lesson-"]')];
  search.addEventListener('input', () => {
    const query = search.value.trim().toLocaleLowerCase();
    let count = 0;
    for (const link of lessonLinks) {
      const matches = `${link.textContent} ${window.LEARNING_COURSE?.title(link.dataset.page) || ''}`.toLocaleLowerCase().includes(query);
      link.hidden = !matches; if (matches) ++count;
    }
    document.getElementById('lesson-search-status').textContent = query ? `${count}개 차시${count ? '' : ' · 검색어를 바꿔 보세요.'}` : '';
  });
  search.addEventListener('keydown', event => {
    if (event.key === 'Escape') { search.value = ''; search.dispatchEvent(new Event('input')); }
  });

  // Horizontal code/table regions must be reachable without a pointing device,
  // and every newly mounted lesson needs the same treatment.
  function decorateReading() {
    for (const region of document.querySelectorAll('article pre, article table')) {
      region.tabIndex = 0;
      if (region.tagName === 'PRE') { region.setAttribute('role', 'region'); region.setAttribute('aria-label', '코드 · 가로 스크롤 가능'); }
    }
    for (const button of document.querySelectorAll('[data-copy]')) {
      const label = button.closest('.codebox')?.querySelector('.codebar span')?.textContent;
      if (label) button.setAttribute('aria-label', `${label} 복사`);
    }
  }
  decorateReading();
  document.addEventListener('learning:mounted', decorateReading);
})();
