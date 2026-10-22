import { useEffect, useState } from 'react';

/**
 * Milliseconds since the animation called `animationId` started, capped at
 * `totalMs`. Re-renders on every browser animation frame until the end.
 */
export function useAnimationClock(animationId: number, totalMs: number): number {
  const [clock, setClock] = useState({ id: animationId, elapsed: 0 });

  useEffect(() => {
    let handle = 0;
    const start = performance.now();
    const tick = (now: number) => {
      // A frame's timestamp can be slightly earlier than `start`, so clamp at 0.
      const elapsed = Math.min(Math.max(now - start, 0), totalMs);
      setClock({ id: animationId, elapsed });
      if (elapsed < totalMs) {
        handle = requestAnimationFrame(tick);
      }
    };
    handle = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(handle);
  }, [animationId, totalMs]);

  // Until the effect has run for a new animation, it is at its start.
  return clock.id === animationId ? clock.elapsed : 0;
}
