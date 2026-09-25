// Tema: kayıtlı tercih → sistem tercihi → açık. İlk stilden önce <head> içinde çalışır.
(function () {
  if (document.documentElement.hasAttribute('data-theme')) return;  // barındırıcı zaten belirlediyse
  var t = null;
  try { t = localStorage.getItem('scada-tema'); } catch (e) {}
  if (t !== 'dark' && t !== 'light') {
    var m = window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)');
    t = m && m.matches ? 'dark' : 'light';
  }
  document.documentElement.setAttribute('data-theme', t);
})();
