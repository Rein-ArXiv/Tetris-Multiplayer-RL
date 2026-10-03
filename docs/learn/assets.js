(function () {
  "use strict";

  // 고정 허용 목록: 호출자가 경로를 지정할 수 없습니다.
  var ASSETS = {
    library: {
      path: "library.js",
      ready: function () { return !!window.LEARNING_LIBRARY; }
    },
    mermaid: {
      path: "vendor/mermaid.min.js",
      ready: function () { return !!window.mermaid; }
    }
  };

  // 이름별 진행 중이거나 성공한 Promise를 기억합니다.
  var pending = Object.create(null);

  function isReady(asset) {
    try { return !!asset.ready(); }
    catch (e) { return false; }
  }

  function buildUrl(path) {
    var site = window.LEARNING_SITE || {};
    var assets = site.assets || {};
    var version = assets[path];
    if (version === undefined || version === null || version === "") {
      version = site.release;
    }
    var url = new URL(path, document.baseURI);
    if (version !== undefined && version !== null && version !== "") {
      url.searchParams.set("v", String(version));
    }
    return url.href;
  }

  function load(name) {
    var asset = Object.prototype.hasOwnProperty.call(ASSETS, name) ? ASSETS[name] : null;
    if (!asset) {
      return Promise.reject(new Error("알 수 없는 학습 자원입니다: " + name));
    }
    // 진행 중이거나 성공한 로딩은 동일한 Promise를 재사용합니다.
    if (pending[name]) {
      return pending[name];
    }
    // 이미 전역이 준비되어 있으면 스크립트 없이 즉시 성공합니다.
    if (isReady(asset)) {
      return Promise.resolve();
    }

    var script;
    var promise = new Promise(function (resolve, reject) {
      script = document.createElement("script");
      script.async = true;
      script.type = "text/javascript";

      // 실패 시 메모를 지우고 태그를 제거해 다음 호출이 재시도할 수 있게 합니다.
      function fail(message) {
        reject(new Error(message));
      }

      script.onload = function () {
        if (isReady(asset)) {
          resolve();
        } else {
          fail("학습 자원을 불러왔지만 전역 개체를 찾을 수 없습니다: " + asset.path);
        }
      };

      script.onerror = function () {
        fail("학습 자원을 불러오지 못했습니다: " + asset.path + " (네트워크 또는 파일 경로를 확인하세요)");
      };

      // 핸들러를 붙인 뒤 src를 설정하고 head에 추가합니다.
      script.src = buildUrl(asset.path);
      document.head.appendChild(script);
    });

    pending[name] = promise.catch(function (error) {
      delete pending[name];
      if (script) {
        script.onload = script.onerror = null;
        if (script.parentNode) script.parentNode.removeChild(script);
      }
      throw error;
    });
    return pending[name];
  }

  var configured = false;
  function diagrams() {
    return load("mermaid").then(function () {
      if (!configured) {
        window.mermaid.initialize({startOnLoad:false,securityLevel:'strict',theme:'default',flowchart:{htmlLabels:false},suppressErrorRendering:true});
        configured = true;
      }
      return window.mermaid;
    });
  }
  window.LEARNING_ASSETS = Object.freeze({ load: load, diagrams: diagrams });
  var serial = 0;
  var rendering = new WeakSet();
  async function enhanceLessons() {
    const items = [...document.querySelectorAll('article[id^="lesson-"] [data-diagram]')];
    if (!items.length) return;
    let engine;
    try { engine = await diagrams(); }
    catch (_) { items.forEach(item => { if (item.isConnected) item.querySelector('details').open = true; }); return; }
    for (const item of items) {
      if (!item.isConnected || item.dataset.diagramReady || rendering.has(item)) continue;
      rendering.add(item);
      try {
        const result = await engine.render('lesson-diagram-' + (++serial), item.querySelector('code').textContent);
        if (item.isConnected) { item.querySelector('.diagram-drawing').innerHTML = result.svg; item.dataset.diagramReady = 'true'; }
      } catch (_) { if (item.isConnected) item.querySelector('details').open = true; }
      finally { rendering.delete(item); }
    }
  }
  document.addEventListener('learning:mounted', enhanceLessons);
})();
