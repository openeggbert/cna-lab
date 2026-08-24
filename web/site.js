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

const lessonChecks = [...document.querySelectorAll('[data-lesson-check]')];
const progress = document.querySelector('[data-tutorial-progress]');
const progressLabel = document.querySelector('[data-progress-label]');
const resetProgress = document.querySelector('[data-reset-progress]');
const progressKey = 'explore2d-tutorial-progress-v1';

function readLessonProgress() {
  try {
    const value = JSON.parse(localStorage.getItem(progressKey) || '[]');
    return Array.isArray(value) ? new Set(value) : new Set();
  } catch {
    return new Set();
  }
}

function writeLessonProgress(completed) {
  try {
    localStorage.setItem(progressKey, JSON.stringify([...completed]));
  } catch {
    // The tutorial remains usable when storage is disabled.
  }
}

function renderLessonProgress(completed) {
  for (const check of lessonChecks) {
    const done = completed.has(check.dataset.lessonCheck);
    check.checked = done;
    check.closest('[data-lesson]')?.classList.toggle('is-complete', done);
  }
  if (progress) progress.value = completed.size;
  if (progressLabel) progressLabel.textContent = `${completed.size} / ${lessonChecks.length}`;
}

if (lessonChecks.length > 0) {
  const completed = readLessonProgress();
  renderLessonProgress(completed);

  for (const check of lessonChecks) {
    check.addEventListener('change', () => {
      if (check.checked) completed.add(check.dataset.lessonCheck);
      else completed.delete(check.dataset.lessonCheck);
      writeLessonProgress(completed);
      renderLessonProgress(completed);
    });
  }

  resetProgress?.addEventListener('click', () => {
    if (!window.confirm('Reset all 24 tutorial checkpoints?')) return;
    completed.clear();
    writeLessonProgress(completed);
    renderLessonProgress(completed);
  });
}
