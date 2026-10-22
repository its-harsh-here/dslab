// Turns the frames of one C++ response into a timed animation. Nothing here
// creates or merges frames: each frame is shown exactly as C++ recorded it.
import type { Frame } from '../engine/protocol';
import { interpolate, layoutFrames, NODE_HEIGHT, NODE_WIDTH, STAGING_DX, type StackLayout } from '../layout/stackLayout';

export interface Animation {
  readonly id: number;
  readonly frames: readonly Frame[];
  readonly layouts: readonly StackLayout[];
  /** What was on screen when the animation started (null on first load). */
  readonly from: StackLayout | null;
  /** Time each frame stays on screen, and how much of it nodes spend moving. */
  readonly frameMs: number;
  readonly moveMs: number;
  /** One viewBox for the whole animation, so the drawing never jumps. */
  readonly viewBox: { readonly x: number; readonly y: number; readonly width: number; readonly height: number };
}

export function createAnimation(id: number, frames: readonly Frame[], from: StackLayout | null): Animation {
  const layouts = layoutFrames(frames, from);
  // About three seconds per operation, but never slower than 550 ms per frame.
  const frameMs = Math.min(550, Math.max(25, 3000 / frames.length));
  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const moveMs = reducedMotion ? 0 : Math.min(350, frameMs * 0.7);
  return { id, frames, layouts, from, frameMs, moveMs, viewBox: viewBoxFor(layouts, from) };
}

export function totalMs(animation: Animation): number {
  return animation.frames.length * animation.frameMs;
}

/** The frame on screen and the node positions at `elapsed` milliseconds. */
export function sample(animation: Animation, elapsed: number): { frame: Frame; layout: StackLayout } {
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

function viewBoxFor(layouts: readonly StackLayout[], from: StackLayout | null) {
  let highest = 0; // the smallest y in use (the stack grows towards negative y)
  for (const layout of from ? [from, ...layouts] : layouts) {
    highest = Math.min(highest, layout.top.y);
    for (const point of layout.nodes.values()) {
      highest = Math.min(highest, point.y);
    }
  }
  const left = -110; // room for the TOP marker
  const right = STAGING_DX + NODE_WIDTH + 90; // room for a waiting node and its pointer label
  const top = highest - 24;
  const bottom = NODE_HEIGHT + 56; // room for the NULL under the bottom node
  return { x: left, y: top, width: right - left, height: bottom - top };
}
