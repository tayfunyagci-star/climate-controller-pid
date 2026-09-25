
// ================================================================= BAŞLATMA
function init() {
  // menü: ikon + görünür metin
  $$('nav.menu a').forEach(a => {
    a.prepend(icon(a.dataset.icon));
    a.addEventListener('click', e => {
      if (e.ctrlKey || e.metaKey || e.shiftKey) return;
      e.preventDefault();
      go(a.dataset.route, true);
      $('#main').focus({preventScroll: true});
    });
  });
  iconize(document);
  // tema
  $('#btn-theme').addEventListener('click', () => {
    const t = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', t);
    lsSet('scada-tema', t);
  });
  // kimlik şeridi: masaüstünde açık, telefonda kapalı
  const idp = $('#identity'), ib = $('#btn-info');
  const setId = open => { idp.classList.toggle('open', open); ib.setAttribute('aria-expanded', open ? 'true' : 'false'); };
  setId(!window.matchMedia('(max-width:600px)').matches);
  ib.addEventListener('click', () => setId(!idp.classList.contains('open')));
  // sistem durumu aç/kapa tercihi
  const dg = $('#diag');
  dg.open = lsGet('scada-diag') === '1';
  dg.addEventListener('toggle', () => { lsSet('scada-diag', dg.open ? '1' : '0'); if (D) renderDiag(); });
  // diyalog
  $('#dlg-ok').addEventListener('click', () => closeDlg(true));
  $('#dlg-cancel').addEventListener('click', () => closeDlg(false));
  $('#dlg-x').addEventListener('click', () => closeDlg(false));
  $('#dlg').addEventListener('cancel', e => { e.preventDefault(); closeDlg(false); });
  window.addEventListener('popstate', () => go(routeFromLocation(), false));
  window.addEventListener('hashchange', () => { if (useHash) go(routeFromLocation(), false); });
  go(routeFromLocation(), false);
  poll();
  setInterval(poll, POLL_MS);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
