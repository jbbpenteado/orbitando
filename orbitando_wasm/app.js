// app.js - production-friendly copy of app_debug.js (title/subtitle injection removed)
document.addEventListener('DOMContentLoaded', function() {
  const rowsContainer = document.getElementById('rows');
  const nInput = document.getElementById('n');
  const buildBtn = document.getElementById('rowsBtn');
  const applyBtn = document.getElementById('applyBtn');
  const startBtn = document.getElementById('startBtn');
  const stopBtn = document.getElementById('stopBtn');
  const canvas = document.getElementById('mainCanvas');

  if (!rowsContainer || !nInput || !buildBtn) {
    console.error('[app] elementos essenciais ausentes:', { rows: !!rowsContainer, n: !!nInput, rowsBtn: !!buildBtn });
    return;
  }

  /* --- header helpers (create, align, observe) --- */
  (function(){
    function createHeader() {
      if (document.getElementById('rows-header')) return;
      const header = document.createElement('div');
      header.id = 'rows-header';
      header.style.display = 'flex';
      header.style.gap = '8px';
      header.style.alignItems = 'center';
      header.style.marginTop = '8px';
      header.style.fontWeight = '700';
      header.style.color = '#DDEEFF';
      header.innerHTML = `
        <div class="hdr-col" data-col="label">Objeto</div>
        <div class="hdr-col" data-col="rx">semi-eixo-x</div>
        <div class="hdr-col" data-col="ry">semi-eixo-y</div>
        <div class="hdr-col" data-col="w">w</div>
        <div class="hdr-col" data-col="s">size</div>
      `;
      Array.from(header.querySelectorAll('.hdr-col')).forEach(c=>{
        c.style.boxSizing = 'border-box';
        c.style.padding = '2px 4px';
        c.style.whiteSpace = 'nowrap';
        c.style.overflow = 'hidden';
        c.style.textOverflow = 'ellipsis';
        c.style.fontSize = '12px'; // headers slightly larger (+2px)
        c.style.lineHeight = '1';
      });
      rowsContainer.parentNode.insertBefore(header, rowsContainer);
    }

    function alignHeader() {
      const header = document.getElementById('rows-header');
      if (!header) return;
      const firstRow = rowsContainer.querySelector('.row');
      if (!firstRow) { header.style.display = 'none'; return; }
      header.style.display = 'flex';
      const hdrCols = header.querySelectorAll('.hdr-col');

      const containerRect = rowsContainer.getBoundingClientRect();
      const labelEl = firstRow.querySelector('label') || firstRow.children[0];
      const rxEl = firstRow.querySelector('.rx');
      const ryEl = firstRow.querySelector('.ry');
      const wEl  = firstRow.querySelector('.w');
      const sEl  = firstRow.querySelector('.s');

      const items = [
        { el: labelEl, key: 'label' },
        { el: rxEl, key: 'rx' },
        { el: ryEl, key: 'ry' },
        { el: wEl,  key: 'w'  },
        { el: sEl,  key: 's'  }
      ];

      const computed = items.map(it => {
        if (!it.el) return { key: it.key, left: null, width: 104 };
        const r = it.el.getBoundingClientRect();
        return { key: it.key, left: Math.round(r.left - containerRect.left), width: Math.max(48, Math.round(r.width)) };
      });

      hdrCols.forEach(col => {
        const key = col.getAttribute('data-col');
        const info = computed.find(c => c.key === key);
        if (!info) return;
        col.style.width = info.width + 'px';
        if (key === 'w' || key === 's') col.style.textAlign = 'center';
        else col.style.textAlign = 'left';
      });

      header.style.width = '100%';
    }

    function observeLayout() {
      let resizeTimer = null;
      window.addEventListener('resize', function(){
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(alignHeader, 80);
      }, { passive: true });

      const mo = new MutationObserver(function(){ alignHeader(); });
      mo.observe(rowsContainer, { childList: true, subtree: false });

      setTimeout(alignHeader, 20);
    }

    window.__orbital_ui_helpers = window.__orbital_ui_helpers || {};
    window.__orbital_ui_helpers.createHeader = createHeader;
    window.__orbital_ui_helpers.alignHeader = alignHeader;
    window.__orbital_ui_helpers.observeLayout = observeLayout;

    createHeader();
    observeLayout();
  })();

  function buildRows() {
    const n = Math.max(1, Math.min(15, parseInt(nInput.value || "1", 10)));
    const builtin_relx = [0.15,0.25,0.35,0.45,0.55,0.65,0.75,0.85,0.95];
    const builtin_rely = [0.12,0.20,0.28,0.35,0.45,0.55,0.65,0.75,0.85];
    const builtin_wv   = [0.8,1.0,1.2,0.6,1.5,0.9,1.3,0.7,1.1];
    const builtin_gs   = [3,4,5,4,6,7,5,8,6];

    rowsContainer.innerHTML = '';
    for (let i = 0; i < n; i++) {
      const relx = (i < builtin_relx.length) ? builtin_relx[i] : (0.15 + 0.08 * i);
      const rely = (i < builtin_rely.length) ? builtin_rely[i] : (0.10 + 0.08 * i);
      const wv   = (i < builtin_wv.length)   ? builtin_wv[i]   : (0.8 + 0.05 * i);
      const gs   = (i < builtin_gs.length)   ? builtin_gs[i]   : 4;

      const div = document.createElement('div');
      div.className = 'row';
      div.style.display = 'flex';
      div.style.gap = '8px';
      div.style.alignItems = 'center';
      div.style.margin = '6px 0';

      div.innerHTML = `
        <label style="width:6.5em; font-size:12px; font-weight:700">Obj ${i+1}</label>
        <input class="rx" type="text" value="${relx.toFixed(2)}" style="width:6.5em">
        <input class="ry" type="text" value="${rely.toFixed(2)}" style="width:6.5em">
        <input class="w"  type="text" value="${wv.toFixed(2)}" style="width:6.5em">
        <input class="s"  type="number" value="${gs * 4}" min="2" max="200" style="width:6.5em">
      `;
      rowsContainer.appendChild(div);
    }

    if (window.__orbital_ui_helpers && typeof window.__orbital_ui_helpers.alignHeader === 'function') {
      setTimeout(window.__orbital_ui_helpers.alignHeader, 20);
    }
  }

  buildBtn.addEventListener('click', function(){ buildRows(); });
  buildRows();

  // WASM runtime readiness and handlers (unchanged)
  function whenReady(cb) {
    if (typeof Module !== 'undefined' && Module['then']) {
      Module.then(cb);
    } else if (typeof Module !== 'undefined' && Module['_start']) {
      cb(Module);
    } else {
      let i = 0;
      const iv = setInterval(function(){
        if (typeof Module !== 'undefined') {
          clearInterval(iv);
          cb(Module);
        }
        if (++i > 200) { clearInterval(iv); console.error('Module not ready'); }
      }, 50);
    }
  }

  whenReady(function(Module){
    // replace canvas if needed and tell wasm canvas size
    try {
      if (Module['canvas'] && Module['canvas'].parentNode) {
        Module['canvas'].parentNode.replaceChild(canvas, Module['canvas']);
        Module['canvas'] = canvas;
      } else {
        Module['canvas'] = canvas;
      }
    } catch (e) { /* ignore */ }

    try {
      if (typeof Module.ccall === 'function') {
        Module.ccall('set_canvas_size','void',['number','number'], [canvas.width, canvas.height]);
      } else if (typeof Module._set_canvas_size === 'function') {
        Module._set_canvas_size(canvas.width, canvas.height);
      }
    } catch (e) { console.warn('set_canvas_size failed', e); }

    // apply / start / stop handlers (same logic as debug)
    applyBtn.addEventListener('click', function(){
      const rows = rowsContainer.querySelectorAll('.row');
      const n = rows.length;
      if (n === 0) { console.warn('no rows to apply'); return; }
      const rxs = new Float64Array(n);
      const rys = new Float64Array(n);
      const ws  = new Float64Array(n);
      const ss  = new Int32Array(n);
      for (let i=0;i<n;i++) {
        const row = rows[i];
        rxs[i] = parseFloat(row.querySelector('.rx').value) || 0.2;
        rys[i] = parseFloat(row.querySelector('.ry').value) || 0.2;
        ws[i]  = parseFloat(row.querySelector('.w').value)  || 1.0;
        ss[i]  = parseInt(row.querySelector('.s').value,10) || 16;
      }
      console.log('[app] apply arrays -> rxs:', rxs, 'rys:', rys, 'ws:', ws, 'ss:', ss);
      try {
        if (typeof Module._malloc !== 'function') { console.error('[app] _malloc not available yet'); return; }
        const bytesF64 = n * 8;
        const ptr_rx = Module._malloc(bytesF64);
        const ptr_ry = Module._malloc(bytesF64);
        const ptr_w  = Module._malloc(bytesF64);
        const ptr_s  = Module._malloc(n * 4);
        Module.HEAPF64.set(rxs, ptr_rx >> 3);
        Module.HEAPF64.set(rys, ptr_ry >> 3);
        Module.HEAPF64.set(ws,  ptr_w  >> 3);
        Module.HEAP32.set(ss,   ptr_s  >> 2);
        let ret;
        if (typeof Module._apply_inputs_from_js === 'function') {
          ret = Module._apply_inputs_from_js(n, ptr_rx, ptr_ry, ptr_w, ptr_s);
        } else if (typeof Module.ccall === 'function') {
          ret = Module.ccall('apply_inputs_from_js','number',['number','number','number','number','number'], [n, ptr_rx, ptr_ry, ptr_w, ptr_s]);
        } else throw new Error('no apply function available');
        console.log('apply_inputs_from_js ->', ret);
      } catch (e) { console.error('apply failed', e); } finally {
        try { if (typeof Module._free === 'function') { Module._free(ptr_rx); Module._free(ptr_ry); Module._free(ptr_w); Module._free(ptr_s); } } catch(e){}
      }
    });

    startBtn.addEventListener('click', function(){
      try {
        if (typeof Module._start_animation === 'function') Module._start_animation();
        else if (typeof Module.ccall === 'function') Module.ccall('start_animation','number',[],[]);
      } catch (e) { console.error('start failed', e); }
    });

    stopBtn.addEventListener('click', function(){
      try {
        if (typeof Module._stop_animation === 'function') Module._stop_animation();
        else if (typeof Module.ccall === 'function') Module.ccall('stop_animation','void',[],[]);
      } catch (e) { console.error('stop failed', e); }
    });
  });
});