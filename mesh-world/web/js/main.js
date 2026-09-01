/* MeshWorld — main.js */

// ── Nav hamburger ──────────────────────────────────────
const hamburger = document.querySelector('.nav-hamburger');
const navLinks  = document.querySelector('.nav-links');

if (hamburger && navLinks) {
  hamburger.addEventListener('click', () => {
    navLinks.classList.toggle('open');
  });
  document.addEventListener('click', (e) => {
    if (!hamburger.contains(e.target) && !navLinks.contains(e.target)) {
      navLinks.classList.remove('open');
    }
  });
}

// ── Active nav link ────────────────────────────────────
(function markActiveNav() {
  const path = location.pathname.split('/').pop() || 'index.html';
  document.querySelectorAll('.nav-links a').forEach(a => {
    const href = a.getAttribute('href');
    if (href === path || (path === '' && href === 'index.html')) {
      a.classList.add('active');
    }
  });
})();

// ── Animated stat counters ─────────────────────────────
function animateCounter(el, target, duration = 1200) {
  const start = performance.now();
  const suffix = el.dataset.suffix || '';
  function tick(now) {
    const elapsed = now - start;
    const progress = Math.min(elapsed / duration, 1);
    const eased = 1 - Math.pow(1 - progress, 3); // ease-out cubic
    el.textContent = Math.round(eased * target) + suffix;
    if (progress < 1) requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
}

const counterObserver = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting && !entry.target.dataset.counted) {
      entry.target.dataset.counted = '1';
      const target = parseInt(entry.target.dataset.target, 10);
      animateCounter(entry.target, target);
    }
  });
}, { threshold: 0.5 });

document.querySelectorAll('.stat-number[data-target]').forEach(el => {
  el.textContent = '0' + (el.dataset.suffix || '');
  counterObserver.observe(el);
});

// ── Fade-in on scroll ──────────────────────────────────
const fadeObserver = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      entry.target.classList.add('visible');
      fadeObserver.unobserve(entry.target);
    }
  });
}, { threshold: 0.1, rootMargin: '0px 0px -40px 0px' });

document.querySelectorAll('.fade-in').forEach(el => fadeObserver.observe(el));

// ── Generator filter ───────────────────────────────────
const filterBtns = document.querySelectorAll('.filter-btn');
const genCards   = document.querySelectorAll('.gen-card');

if (filterBtns.length && genCards.length) {
  filterBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      filterBtns.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      const filter = btn.dataset.filter;
      genCards.forEach(card => {
        const show = filter === 'all' || card.dataset.tags?.split(' ').includes(filter);
        card.style.display = show ? '' : 'none';
      });
    });
  });
}

// ── Code copy buttons ──────────────────────────────────
document.querySelectorAll('.code-copy').forEach(btn => {
  btn.addEventListener('click', () => {
    const pre = btn.closest('.code-block')?.querySelector('pre');
    if (!pre) return;
    navigator.clipboard.writeText(pre.innerText).then(() => {
      const orig = btn.textContent;
      btn.textContent = 'Copied!';
      btn.style.color = 'var(--accent3)';
      setTimeout(() => { btn.textContent = orig; btn.style.color = ''; }, 1800);
    });
  });
});

// ── Smooth external link handling ──────────────────────
document.querySelectorAll('a[href^="http"]').forEach(a => {
  if (!a.hasAttribute('target')) {
    a.setAttribute('target', '_blank');
    a.setAttribute('rel', 'noopener noreferrer');
  }
});
