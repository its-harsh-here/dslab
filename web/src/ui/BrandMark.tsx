// The DSLab logo in the header: a three-node tree that reacts to everything.
// It leans towards the pointer, spreads its children on hover, runs faster
// while an operation plays, and does a full rotation when clicked. All of
// the motion is CSS (styles.css, "The logo"); this file only sets classes and
// the lean.
import { useEffect, useRef, useState } from 'react';

export function BrandMark({ busy, onCelebrate }: { busy: boolean; onCelebrate?: () => void }) {
  const svg = useRef<SVGSVGElement>(null);
  const [spins, setSpins] = useState(0);
  const [celebrating, setCelebrating] = useState(false);
  const timer = useRef<number | undefined>(undefined);

  // Lean a little towards the pointer, wherever it is on the page.
  useEffect(() => {
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    let frame = 0;
    function onMove(event: PointerEvent) {
      cancelAnimationFrame(frame);
      frame = requestAnimationFrame(() => {
        const element = svg.current;
        if (!element) return;
        const box = element.getBoundingClientRect();
        const dx = event.clientX - (box.left + box.width / 2);
        const dy = event.clientY - (box.top + box.height / 2);
        const distance = Math.hypot(dx, dy) || 1;
        const pull = Math.min(2.5, distance / 80);
        element.style.setProperty('--lean-x', `${((dx / distance) * pull).toFixed(2)}px`);
        element.style.setProperty('--lean-y', `${((dy / distance) * pull).toFixed(2)}px`);
      });
    }
    window.addEventListener('pointermove', onMove);
    return () => {
      window.removeEventListener('pointermove', onMove);
      cancelAnimationFrame(frame);
    };
  }, []);

  useEffect(() => () => window.clearTimeout(timer.current), []);

  function celebrate() {
    setSpins((count) => count + 1); // a new key restarts the animation on every click
    setCelebrating(true);
    window.clearTimeout(timer.current);
    timer.current = window.setTimeout(() => setCelebrating(false), 950);
    onCelebrate?.();
  }

  const classes = ['brand-mark'];
  if (busy) classes.push('busy');
  if (celebrating) classes.push('celebrate');

  return (
    <button type="button" className={busy ? 'brand-button busy' : 'brand-button'} onClick={celebrate}
      aria-label="DSLab logo: play its animation">
      <svg ref={svg} className={classes.join(' ')} viewBox="0 0 32 32" aria-hidden="true" focusable="false">
        <g className="tree">
          <g className="spinner-group" key={spins}>
            <g className="child left">
              <line x1={16} y1={9} x2={8.5} y2={22.5} />
              <circle className="m1" cx={8.5} cy={22.5} r={4.5} />
            </g>
            <g className="child right">
              <line x1={16} y1={9} x2={23.5} y2={22.5} />
              <circle className="m2" cx={23.5} cy={22.5} r={4.5} />
            </g>
            <circle className="m0" cx={16} cy={9} r={4.5} />
          </g>
        </g>
      </svg>
    </button>
  );
}
