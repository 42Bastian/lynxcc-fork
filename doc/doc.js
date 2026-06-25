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

// Tools dropdown: hover/focus opens it via CSS; this adds click-to-toggle
// (touch + keyboard) and closes the menu on an outside click or Escape.
(function () {
  var drop = document.querySelector('.topbar .nav .dropdown');
  if (!drop) return;
  var trigger = drop.querySelector('.dropbtn');
  function close() {
    drop.classList.remove('open');
    if (trigger) trigger.setAttribute('aria-expanded', 'false');
  }
  if (trigger) {
    trigger.addEventListener('click', function (e) {
      e.preventDefault();
      var open = drop.classList.toggle('open');
      trigger.setAttribute('aria-expanded', open ? 'true' : 'false');
    });
  }
  document.addEventListener('click', function (e) {
    if (!drop.contains(e.target)) close();
  });
  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') close();
  });
})();

// Function reference: live filter for the left-hand function index, and
// highlight of the entry currently scrolled into view.
(function () {
  var input = document.getElementById('fnFilter');
  var index = document.getElementById('fnIndex');
  if (!input || !index) return;
  var items = Array.prototype.slice.call(index.querySelectorAll('li'));
  var empty = document.querySelector('.fn-empty');
  input.addEventListener('input', function () {
    var q = input.value.trim().toLowerCase();
    var shown = 0;
    items.forEach(function (li) {
      var hit = !q || li.getAttribute('data-name').indexOf(q) !== -1;
      li.classList.toggle('fn-hidden', !hit);
      if (hit) shown++;
    });
    if (empty) empty.style.display = shown ? 'none' : 'block';
  });

  var links = {};
  items.forEach(function (li) {
    var a = li.querySelector('a');
    if (a) links[a.getAttribute('href').slice(1)] = a;
  });
  var current = null;
  var headings = Array.prototype.slice.call(
    document.querySelectorAll('.fn-main h3[id]'));
  if (!('IntersectionObserver' in window) || !headings.length) return;
  var io = new IntersectionObserver(function (entries) {
    entries.forEach(function (en) {
      if (!en.isIntersecting) return;
      var a = links[en.target.id];
      if (!a || a === current) return;
      if (current) current.classList.remove('current');
      a.classList.add('current');
      current = a;
      a.scrollIntoView({ block: 'nearest' });
    });
  }, { rootMargin: '-60px 0px -75% 0px' });
  headings.forEach(function (h) { io.observe(h); });
})();
