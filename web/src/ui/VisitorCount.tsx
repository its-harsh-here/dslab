// "Used by 1K+ visitors" on the overview page.
//
// The number comes from web/public/stats.json, served next to the page. Today
// it is set by hand. Later a scheduled job can write it from Google Analytics
// (README, "Visitor count"): a static site cannot ask Google Analytics itself,
// because that needs a private key that must never reach the browser.
//
// The count rolls up from zero when the page opens (not under reduced motion).
// If the file is missing or malformed, nothing is shown.
import { useEffect, useState } from 'react';

interface Stats {
  readonly visitors: number;
}

const COUNT_UP_MS = 1400;

/** 1000 → "1K+", 2500 → "2.5K+", 12000 → "12K+", 640 → "640+". */
export function formatVisitors(count: number): string {
  return `${new Intl.NumberFormat('en', { notation: 'compact', maximumFractionDigits: 1 }).format(count)}+`;
}

export function VisitorCount() {
  const [target, setTarget] = useState<number | null>(null);
  const [shown, setShown] = useState(0);

  useEffect(() => {
    let active = true;
    fetch(`${import.meta.env.BASE_URL}stats.json`, { cache: 'no-cache' })
      .then((response) => (response.ok ? (response.json() as Promise<Stats>) : null))
      .then((stats) => {
        if (active && stats && Number.isFinite(stats.visitors) && stats.visitors > 0) setTarget(Math.floor(stats.visitors));
      })
      .catch(() => {
        // No count: the line is simply not shown.
      });
    return () => {
      active = false;
    };
  }, []);

  useEffect(() => {
    if (target === null) return;
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      setShown(target);
      return;
    }
    let frame = 0;
    const start = performance.now();
    const step = (now: number) => {
      const t = Math.min(1, Math.max(0, (now - start) / COUNT_UP_MS));
      const eased = 1 - (1 - t) ** 3;
      setShown(Math.round(target * eased));
      if (t < 1) frame = requestAnimationFrame(step);
    };
    frame = requestAnimationFrame(step);
    return () => cancelAnimationFrame(frame);
  }, [target]);

  if (target === null) return null;
  return (
    <p className="visitor-count" aria-label={`Used by ${formatVisitors(target)} visitors`}>
      <span className="live-dot" aria-hidden="true" />
      Used by <strong aria-hidden="true">{formatVisitors(shown)}</strong> visitors
    </p>
  );
}
