// Turns the frames of one C++ response into a timed animation. Nothing here
// creates or merges frames: each frame is shown exactly as C++ recorded it.
import type { Frame, Snapshot } from '../engine/protocol';
import { interpolate, layoutFrames, viewBoxFor, type Layout, type LayoutStrategy, type ViewBox } from '../layout/layout';

export interface Animation<S extends Snapshot> {
  readonly id: number;
  readonly frames: readonly Frame<S>[];
  readonly layouts: readonly Layout[];
  /** What was on screen when the animation started (null on first load). */
  readonly from: Layout | null;
  /** Time each frame stays on screen, and how much of it nodes spend moving. */
  readonly frameMs: number;
  readonly moveMs: number;
  /** One viewBox for the whole animation, so the drawing never jumps. */
  readonly viewBox: ViewBox;
}

export function createAnimation<S extends Snapshot>(
  id: number,
  frames: readonly Frame<S>[],
  from: Layout | null,
  strategy: LayoutStrategy<S>,
): Animation<S> {
  const layouts = layoutFrames(frames, from, strategy);
  // About three seconds per operation, but never slower than 550 ms per frame.
  const frameMs = Math.min(550, Math.max(25, 3000 / frames.length));
  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const moveMs = reducedMotion ? 0 : Math.min(350, frameMs * 0.7);
  const viewBox = viewBoxFor(from ? [from, ...layouts] : layouts, strategy.box);
  return { id, frames, layouts, from, frameMs, moveMs, viewBox };
}

export function totalMs(animation: Animation<Snapshot>): number {
  return animation.frames.length * animation.frameMs;
}

/** The frame on screen and the node positions at `elapsed` milliseconds. */
export function sample<S extends Snapshot>(animation: Animation<S>, elapsed: number): { frame: Frame<S>; layout: Layout } {
  const last = animation.frames.length - 1;
  const index = Math.max(0, Math.min(Math.floor(elapsed / animation.frameMs), last));
  const intoFrame = elapsed - index * animation.frameMs;
  const t = animation.moveMs === 0 ? 1 : Math.min(1, intoFrame / animation.moveMs);
  const from = index === 0 ? animation.from : animation.layouts[index - 1];
  return { frame: animation.frames[index], layout: interpolate(from, animation.layouts[index], ease(t)) };
}

function ease(t: number): number {
  return t < 0.5 ? 2 * t * t : 1 - (-2 * t + 2) ** 2 / 2;
}
