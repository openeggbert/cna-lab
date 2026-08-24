const navToggle = document.querySelector('.nav-toggle');
const nav = document.querySelector('.site-nav');

navToggle?.addEventListener('click', () => {
  const open = nav.classList.toggle('open');
  navToggle.setAttribute('aria-expanded', String(open));
});

nav?.addEventListener('click', (event) => {
  if (event.target instanceof HTMLAnchorElement) {
    nav.classList.remove('open');
    navToggle?.setAttribute('aria-expanded', 'false');
  }
});

for (const button of document.querySelectorAll('[data-copy]')) {
  button.addEventListener('click', async () => {
    const source = document.getElementById(button.dataset.copy);
    if (!source) return;
    const previous = button.textContent;
    try {
      await navigator.clipboard.writeText(source.innerText);
      button.textContent = 'Copied';
    } catch {
      button.textContent = 'Select code';
    }
    window.setTimeout(() => { button.textContent = previous; }, 1400);
  });
}

const year = document.getElementById('year');
if (year) year.textContent = String(new Date().getFullYear());
