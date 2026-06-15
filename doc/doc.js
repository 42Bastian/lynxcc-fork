// Theme toggle, persisted in localStorage. The initial theme is applied by an
// inline script in <head> to avoid a flash of the wrong theme.
(function () {
  function current() {
    return document.documentElement.getAttribute('data-theme') ||
      (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches
        ? 'dark' : 'light');
  }
  function apply(t) {
    document.documentElement.setAttribute('data-theme', t);
    try { localStorage.setItem('cc65-theme', t); } catch (e) {}
  }
  var btn = document.getElementById('themeToggle');
  if (btn) {
    btn.addEventListener('click', function () {
      apply(current() === 'dark' ? 'light' : 'dark');
    });
  }
})();
