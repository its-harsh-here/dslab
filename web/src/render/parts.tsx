// Small SVG building blocks shared by the structure drawings.
import { useId, type KeyboardEvent } from 'react';

import type { NodeId } from '../engine/protocol';
import type { Point } from '../layout/layout';

/** `markerEnd` values for a normal and a highlighted arrow head. */
export interface Arrows {
  readonly normal: string;
  readonly hot: string;
  readonly normalId: string;
  readonly hotId: string;
}

/**
 * Arrow-head marker IDs that are unique to one drawing. Every tab stays in the
 * page, so a shared id="arrow" would point at a marker inside a hidden tab,
 * which browsers do not draw.
 */
export function useArrows(): Arrows {
  const unique = useId().replace(/[^a-zA-Z0-9_-]/g, '');
  const normalId = `arrow-${unique}`;
  const hotId = `arrow-hot-${unique}`;
  return { normal: `url(#${normalId})`, hot: `url(#${hotId})`, normalId, hotId };
}

/** The arrow-head markers; put once inside each <svg>. */
export function ArrowDefs({ arrows }: { arrows: Arrows }) {
  return (
    <defs>
      <marker id={arrows.normalId} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
        <path d="M 0 0 L 10 5 L 0 10 z" className="arrow-head" />
      </marker>
      <marker id={arrows.hotId} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
        <path d="M 0 0 L 10 5 L 0 10 z" className="arrow-head hot" />
      </marker>
    </defs>
  );
}

interface NodeState {
  highlighted: boolean;
  /** Not reachable from the entry pointer in this frame (being linked in or out). */
  detached: boolean;
  selected?: boolean;
}

export function nodeClasses(base: string, state: NodeState): string {
  const classes = [base];
  if (state.highlighted) classes.push('highlighted');
  if (state.detached) classes.push('detached');
  if (state.selected) classes.push('selected');
  return classes.join(' ');
}

/** Props that make a node clickable and keyboard-selectable by its stable ID. */
export function selectableProps(id: NodeId, value: number, selected: boolean, onSelect?: (id: NodeId) => void) {
  if (!onSelect) return {};
  return {
    role: 'button',
    tabIndex: 0,
    'aria-pressed': selected,
    'aria-label': `Node #${id} holding ${value}`,
    onClick: () => onSelect(id),
    onKeyDown: (event: KeyboardEvent) => {
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        onSelect(id);
      }
    },
  };
}

interface BoxNodeProps extends NodeState {
  id: NodeId;
  value: number;
  at: Point;
  width: number;
  height: number;
  onSelect?: (id: NodeId) => void;
}

/** A node drawn as a box with its value, and its ID in small print. */
export function BoxNode({ id, value, at, width, height, onSelect, ...state }: BoxNodeProps) {
  return (
    <g
      data-node-id={id}
      className={nodeClasses('node', state)}
      transform={`translate(${at.x} ${at.y})`}
      {...selectableProps(id, value, state.selected ?? false, onSelect)}
    >
      <rect width={width} height={height} rx={6} />
      <text className="node-id" x={8} y={14}>#{id}</text>
      <text className="node-value" x={width / 2} y={height / 2 + 6} textAnchor="middle">
        {value}
      </text>
    </g>
  );
}

/** The algorithm's own variables ("node", "curr", ...), stacked at a spot next to each node. */
export function PointerLabels({
  pointers,
  nodes,
  spot,
}: {
  pointers: Readonly<Record<string, NodeId | null>>;
  nodes: ReadonlyMap<NodeId, Point>;
  spot: (at: Point, line: number) => Point;
}) {
  const linesUsed = new Map<NodeId, number>();
  return (
    <>
      {Object.entries(pointers).map(([name, id]) => {
        const at = id === null ? undefined : nodes.get(id);
        if (id === null || !at) return null;
        const line = linesUsed.get(id) ?? 0;
        linesUsed.set(id, line + 1);
        const place = spot(at, line);
        return (
          <text key={name} className="pointer-label" x={place.x} y={place.y}>
            {name}
          </text>
        );
      })}
    </>
  );
}
