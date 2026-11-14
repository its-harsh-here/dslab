// The browser-tab icon is alive too. While an operation plays, its three
// nodes light up in turn (like a traversal), so the tab shows activity even
// when another tab is in front; a click on the logo makes it spin. When
// nothing happens it is the static icon from web/public/favicon.svg again.
import { useEffect, useRef } from 'react';

export type FaviconMode = 'idle' | 'busy' | 'celebrate';

const COLORS = ['#4f46e5', '#06b6d4', '#d946ef'];

/** One frame of the tab icon: node `lit` (0 root, 1 left, 2 right) glows; the tree is turned by `angle`. */
function frame(lit: number, angle: number): string {
  const nodes: [number, number][] = [[16, 9], [9.5, 22.5], [22.5, 22.5]];
  const circles = nodes
    .map(([x, y], index) => {
      const on = index === lit;
      return `<circle cx="${x}" cy="${y}" r="${on ? 5.6 : 4.3}" fill="${on ? '#ffffff' : 'rgba(255,255,255,0.72)'}"/>`;
    })
    .join('');
  const svg =
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">` +
    `<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">` +
    `<stop offset="0" stop-color="${COLORS[0]}"/><stop offset="1" stop-color="${COLORS[(lit + 1) % 3] ?? COLORS[1]}"/>` +
    `</linearGradient></defs><rect width="32" height="32" rx="8" fill="url(#g)"/>` +
    `<g transform="rotate(${angle} 16 16)"><path d="M16 9 9.5 22.5M16 9l6.5 13.5" stroke="#ffffff" stroke-width="2.4" ` +
    `stroke-linecap="round"/>${circles}</g></svg>`;
  return `data:image/svg+xml,${encodeURIComponent(svg)}`;
}

export function useFavicon(mode: FaviconMode) {
  // The href index.html set, restored exactly whenever the app is idle.
  const original = useRef<string | null>(null);

  useEffect(() => {
    const link = document.querySelector<HTMLLinkElement>('link[rel="icon"]');
    if (!link) return;
    if (original.current === null) original.current = link.getAttribute('href');
    const restore = () => {
      if (original.current !== null) link.setAttribute('href', original.current);
    };
    if (mode === 'idle' || window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
      restore();
      return;
    }

    let step = 0;
    const tick = () => {
      if (mode === 'busy') {
        link.setAttribute('href', frame(step % 3, 0));
      } else {
        link.setAttribute('href', frame(step % 3, (step * 45) % 360));
      }
      step += 1;
    };
    tick();
    const interval = window.setInterval(tick, mode === 'busy' ? 240 : 90);
    return () => {
      window.clearInterval(interval);
      restore();
    };
  }, [mode]);
}
