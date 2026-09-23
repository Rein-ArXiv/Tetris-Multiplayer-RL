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
  const prefs = { size: 'normal', theme: 'system', wrap: false, location: '' };
  const validLocation = hash => typeof hash === 'string' && /^lesson-\d+(--[a-z0-9-]+)?$/.test(hash)
    && Boolean(window.LEARNING_COURSE?.anchorLabel(hash));
  try {
    const saved = JSON.parse(localStorage.getItem(storageKey) || 'null');
    if (saved && typeof saved === 'object') {
      if (['normal', 'large', 'larger'].includes(saved.size)) prefs.size = saved.size;
      if (['system', 'light', 'dark'].includes(saved.theme)) prefs.theme = saved.theme;
      prefs.wrap = saved.wrap === true;
      if (validLocation(saved.location)) prefs.location = saved.location;
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
  function showResume() {
    resume.hidden = !prefs.location;
    document.getElementById('resume-empty').hidden = Boolean(prefs.location);
    if (!prefs.location) return;
    const [page, anchor] = prefs.location.split('--');
    const course = window.LEARNING_COURSE;
    const label = anchor ? course?.anchorLabel(prefs.location) : course?.title(page);
    resume.href = `#${prefs.location}`;
    resume.textContent = `최근 방문: ${page.split('-')[1]}차시 · ${label || '본문'}`;
  }
  for (const control of [size, theme, wrap]) control.addEventListener('change', () => {
    prefs.size = size.value; prefs.theme = theme.value; prefs.wrap = wrap.checked;
    apply(); save();
  });
  apply(); showResume();
  // Keep the previous visit available on first load; an explicit link wins over it.
  window.addEventListener('hashchange', () => {
    const hash = location.hash.slice(1);
    if (!validLocation(hash)) return;
    prefs.location = hash; save(); showResume();
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
