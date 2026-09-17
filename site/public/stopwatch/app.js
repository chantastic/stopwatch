const skillURL = new URL('/stopwatch/SKILL.md', window.location.href).href;
document.getElementById('skill-address').textContent = skillURL;
const copyButton = document.getElementById('copy-prompt');
const copyStatus = document.getElementById('copy-status');
copyButton.addEventListener('click', async () => {
  const prompt = document.getElementById('agent-prompt').textContent;
  try {
    await navigator.clipboard.writeText(prompt);
    copyStatus.textContent = 'Prompt copied. Ready for your agent.';
    copyButton.textContent = 'Copied ✓';
    setTimeout(() => { copyButton.innerHTML = 'Copy prompt <span aria-hidden="true">⧉</span>'; }, 2500);
  } catch {
    const selection = window.getSelection();
    const range = document.createRange();
    range.selectNodeContents(document.getElementById('agent-prompt'));
    selection.removeAllRanges();
    selection.addRange(range);
    copyStatus.textContent = 'Prompt selected. Use your device’s copy command.';
  }
});

const links = [...document.querySelectorAll('nav a')];
if ('IntersectionObserver' in window) {
  const observer = new IntersectionObserver(entries => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        for (const link of links) {
          const current = link.hash === `#${entry.target.id}`;
          link.classList.toggle('active', current);
          if (current) link.setAttribute('aria-current', 'location');
          else link.removeAttribute('aria-current');
        }
      }
    }
  }, { rootMargin: '-25% 0px -55% 0px' });
  document.querySelectorAll('main section[id]').forEach(section => observer.observe(section));
}
