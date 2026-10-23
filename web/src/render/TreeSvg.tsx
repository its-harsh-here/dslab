// Draws one moment of a binary tree, BST, AVL or red-black animation. Child
// links are labelled L and R; parent pointers are stored by C++ but not drawn
// (they mirror the child links). AVL nodes also show their height and balance;
// a balance outside -1..+1 (a temporary imbalance) is drawn in red. Red-black
// nodes are filled with their color and carry an R or B badge, so the color
// never depends on seeing red; `showNil` also draws the black NIL leaves.
import { edgeId, type Frame, type NodeId, type TreeSnapshot } from '../engine/protocol';
import type { Layout, Point, ViewBox } from '../layout/layout';
import { TREE_RADIUS as R, treeReachable } from '../layout/tree';
import { ArrowDefs, nodeClasses, PointerLabels, selectableProps, useArrows } from './parts';

interface Props {
  frame: Frame<TreeSnapshot>;
  layout: Layout;
  viewBox: ViewBox;
  label: string;
  selectedId?: NodeId | null;
  onSelect?: (id: NodeId) => void;
  /** Red-black: draw a NIL leaf for every missing child of a node in the tree. */
  showNil?: boolean;
}

// A NIL leaf sits this far left or right of its parent's centre, and this far below it.
const NIL_DX = 26;
const NIL_TOP = 42;

export function TreeSvg({ frame, layout, viewBox, label, selectedId, onSelect, showNil = false }: Props) {
  const arrows = useArrows();
  const { snapshot, highlight, pointers } = frame;
  const reachable = treeReachable(snapshot);
  const hotNodes = new Set(highlight.nodes);
  const hotEdges = new Set(highlight.edges);
  const root = layout.markers.get('ROOT') ?? { x: 0, y: 0 };

  const links: { from: NodeId; to: NodeId; side: 'L' | 'R' }[] = [];
  // The NIL leaves: missing children of nodes in the tree (detached nodes have none).
  const nilLeaves: { parent: NodeId; side: 'L' | 'R' }[] = [];
  for (const node of snapshot.nodes) {
    if (node.left !== null) links.push({ from: node.id, to: node.left, side: 'L' });
    if (node.right !== null) links.push({ from: node.id, to: node.right, side: 'R' });
    if (showNil && reachable.has(node.id)) {
      if (node.left === null) nilLeaves.push({ parent: node.id, side: 'L' });
      if (node.right === null) nilLeaves.push({ parent: node.id, side: 'R' });
    }
  }

  return (
    <svg className="structure-svg" role="group" aria-label={label} width="100%" height="100%"
      preserveAspectRatio="xMidYMid meet" viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}>
      <ArrowDefs arrows={arrows} />

      {links.map(({ from, to, side }) => {
        const a = layout.nodes.get(from);
        const b = layout.nodes.get(to);
        if (!a || !b) return null;
        const id = edgeId(from, to);
        const hot = hotEdges.has(id);
        const [start, end] = trim(a, b, R);
        const labelAt = sideLabel(a, b, side);
        return (
          <g key={id}>
            <line data-edge-id={id} className={hot ? 'edge highlighted' : 'edge'}
              x1={start.x} y1={start.y} x2={end.x} y2={end.y} markerEnd={hot ? arrows.hot : arrows.normal} />
            <text className={hot ? 'edge-label highlighted' : 'edge-label'} x={labelAt.x} y={labelAt.y} textAnchor="middle">
              {side}
            </text>
          </g>
        );
      })}

      {nilLeaves.map(({ parent, side }) => {
        const at = layout.nodes.get(parent);
        if (!at) return null;
        const dx = side === 'L' ? -NIL_DX : NIL_DX;
        const length = Math.hypot(dx, NIL_TOP);
        return (
          <g key={`nil-${parent}-${side}`} className="nil-leaf" data-nil-of={parent} data-side={side}>
            <line x1={at.x + (dx / length) * R} y1={at.y + (NIL_TOP / length) * R} x2={at.x + dx} y2={at.y + NIL_TOP} />
            <rect x={at.x + dx - 12} y={at.y + NIL_TOP} width={24} height={16} rx={2} />
            <text x={at.x + dx} y={at.y + NIL_TOP + 12} textAnchor="middle">NIL</text>
          </g>
        );
      })}

      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        if (!at) return null;
        const selected = selectedId === node.id;
        const unbalanced = node.balance !== undefined && Math.abs(node.balance) > 1;
        let classes = nodeClasses('node', { highlighted: hotNodes.has(node.id), detached: !reachable.has(node.id), selected });
        if (unbalanced) classes += ' unbalanced';
        if (node.color) classes += node.color === 'red' ? ' rb-red' : ' rb-black';
        return (
          <g key={node.id} data-node-id={node.id} data-color={node.color} transform={`translate(${at.x} ${at.y})`}
            className={classes}
            {...selectableProps(node.id, node.value, selected, onSelect, node.color)}>
            {selected && <circle className="selection-ring" r={R + 5} />}
            <circle className="node-body" r={R} />
            <text className="node-value" y={6} textAnchor="middle">{node.value}</text>
            <text className="node-id" y={R + 13} textAnchor="middle">#{node.id}</text>
            {node.color && (
              <g className="rb-badge" transform={`translate(${-R - 10} 0)`} aria-hidden="true">
                <circle r={8} />
                <text y={3.5} textAnchor="middle">{node.color === 'red' ? 'R' : 'B'}</text>
              </g>
            )}
            {node.height !== undefined && node.balance !== undefined && (
              <text className={unbalanced ? 'avl-label unbalanced' : 'avl-label'} data-avl-label y={R + 26} textAnchor="middle">
                h{node.height} · bf {node.balance > 0 ? `+${node.balance}` : node.balance}
              </text>
            )}
          </g>
        );
      })}

      <g className="marker" data-marker="ROOT">
        <text x={root.x} y={root.y - 52} textAnchor="middle">ROOT</text>
        <line x1={root.x} y1={root.y - 46} x2={root.x} y2={root.y - R - 4} markerEnd={arrows.normal} />
        {snapshot.root === null && (
          <text className="null-text" x={root.x} y={root.y + 4} textAnchor="middle">{snapshot.nilColor ? 'NIL' : 'NULL'}</text>
        )}
      </g>

      <PointerLabels pointers={pointers} nodes={layout.nodes}
        spot={(at, line) => ({ x: at.x + R + 6, y: at.y + 4 + line * 13 })} />
    </svg>
  );
}

/** The segment between two circle centres, shortened so it starts and ends at the circles' edges. */
function trim(a: Point, b: Point, radius: number): [Point, Point] {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const length = Math.hypot(dx, dy) || 1;
  const ux = dx / length;
  const uy = dy / length;
  return [
    { x: a.x + ux * radius, y: a.y + uy * radius },
    { x: b.x - ux * (radius + 2), y: b.y - uy * (radius + 2) },
  ];
}

/** Where the L or R label sits: 40% along the edge, pushed outwards to the edge's side. */
function sideLabel(a: Point, b: Point, side: 'L' | 'R'): Point {
  const x = a.x + (b.x - a.x) * 0.4;
  const y = a.y + (b.y - a.y) * 0.4;
  return { x: x + (side === 'L' ? -10 : 10), y: y - 2 };
}
