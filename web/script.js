const districtButtons = document.querySelectorAll('[data-district]');
const districtPanels = document.querySelectorAll('.district-panel');

districtButtons.forEach((button) => {
  button.addEventListener('click', () => {
    districtButtons.forEach((item) => item.classList.toggle('active', item === button));
    districtPanels.forEach((panel) => panel.classList.toggle('active', panel.id === button.dataset.district));
  });
});
