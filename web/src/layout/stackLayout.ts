// Where each stack node is drawn, frame by frame (docs/architecture.md, 3.2).
// Layout only reads the links in a snapshot to place nodes. It never changes
// a link and never decides the outcome of an operation: C++ did that already.
import type { Frame, NodeId } from '../engine/protocol';

export interface Point {
  readonly x: number;
  readonly y: number;
}

export interface StackLayout {
  readonly nodes: ReadonlyMap<NodeId, Point>;
  /** Where the TOP marker points: the top node, or the empty bottom slot. */
  readonly top: Point;
}

// Sizes in SVG user units.
export const NODE_WIDTH = 120;
export const NODE_HEIGHT = 40;
export const ROW = 64; // distance between two stacked nodes
export const STAGING_DX = 180; // how far right a node waits before it is linked in

/** IDs reachable from top by following `next`, top first. */
export function chainFromTop(frame: Frame): NodeId[] {
  const nextOf = new Map(frame.snapshot.nodes.map((node) => [node.id, node.next]));
  const chain: NodeId[] = [];
  const seen = new Set<NodeId>();
  for (let id = frame.snapshot.top; id !== null && !seen.has(id); id = nextOf.get(id) ?? null) {
    chain.push(id);
    seen.add(id);
  }
  return chain;
}

/**
 * Positions for one frame. The stack grows upwards: the bottom node is at y = 0
 * and each node above it is one ROW higher (more negative y).
 *  - A node reachable from top gets its slot.
 *  - A node that is not reachable in this frame (just created, or just
 *    unlinked by pop) keeps its position from the previous frame.
 *  - A new node with no previous position waits to the right of the slot
 *    above the current top, at the height where it will end up.
 */
export function layoutFrame(frame: Frame, previous: StackLayout | null): StackLayout {
  const chain = chainFromTop(frame);
  const nodes = new Map<NodeId, Point>();
  chain.forEach((id, index) => {
    const slotFromBottom = chain.length - 1 - index;
    nodes.set(id, { x: 0, y: -slotFromBottom * ROW });
  });

  const top = chain.length > 0 ? nodes.get(chain[0])! : { x: 0, y: 0 };
  const staging = { x: STAGING_DX, y: chain.length > 0 ? top.y - ROW : 0 };
  for (const node of frame.snapshot.nodes) {
    if (!nodes.has(node.id)) {
      nodes.set(node.id, previous?.nodes.get(node.id) ?? staging);
    }
  }
  return { nodes, top };
}

/** Layouts for every frame of one response, each continuing from the one before. */
export function layoutFrames(frames: readonly Frame[], start: StackLayout | null): StackLayout[] {
  const layouts: StackLayout[] = [];
  let previous = start;
  for (const frame of frames) {
    previous = layoutFrame(frame, previous);
    layouts.push(previous);
  }
  return layouts;
}

/** A layout part of the way (t from 0 to 1) from `from` to `to`. */
export function interpolate(from: StackLayout | null, to: StackLayout, t: number): StackLayout {
  if (from === null || t >= 1) {
    return to;
  }
  const nodes = new Map<NodeId, Point>();
  for (const [id, end] of to.nodes) {
    nodes.set(id, mix(from.nodes.get(id) ?? end, end, t));
  }
  return { nodes, top: mix(from.top, to.top, t) };
}

function mix(a: Point, b: Point, t: number): Point {
  return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
}
