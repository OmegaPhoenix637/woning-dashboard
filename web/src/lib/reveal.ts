// Laat elementen met class "rise" rustig in beeld glijden zodra ze zichtbaar worden.
export function startReveal() {
  const root = document.documentElement;
  // Bij geautomatiseerde screenshots of zonder IntersectionObserver: alles direct tonen.
  if (navigator.webdriver || !('IntersectionObserver' in window)) {
    root.classList.add('no-reveal');
    return;
  }
  const io = new IntersectionObserver(
    (entries) => {
      for (const e of entries) {
        if (e.isIntersecting) {
          e.target.classList.add('in');
          io.unobserve(e.target);
        }
      }
    },
    { rootMargin: '0px 0px -6% 0px', threshold: 0.06 },
  );
  const scan = () => document.querySelectorAll('.rise:not(.in)').forEach((el) => io.observe(el));
  scan();
  new MutationObserver(scan).observe(document.body, { childList: true, subtree: true });
}
