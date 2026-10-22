// Draws one moment of a binary tree or BST animation. Child links are labelled
// L and R; parent pointers are stored by C++ but not drawn (they mirror the
// child links).
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
}

export function TreeSvg({ frame, layout, viewBox, label, selectedId, onSelect }: Props) {
  const arrows = useArrows();
  const { snapshot, highlight, pointers } = frame;
  const reachable = treeReachable(snapshot);
  const hotNodes = new Set(highlight.nodes);
  const hotEdges = new Set(highlight.edges);
  const root = layout.markers.get('ROOT') ?? { x: 0, y: 0 };

  const links: { from: NodeId; to: NodeId; side: 'L' | 'R' }[] = [];
  for (const node of snapshot.nodes) {
    if (node.left !== null) links.push({ from: node.id, to: node.left, side: 'L' });
    if (node.right !== null) links.push({ from: node.id, to: node.right, side: 'R' });
  }

  return (
    <svg className="structure-svg" role="img" aria-label={label} width={viewBox.width} height={viewBox.height}
      viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}>
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

      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        if (!at) return null;
        const selected = selectedId === node.id;
        return (
          <g key={node.id} data-node-id={node.id} transform={`translate(${at.x} ${at.y})`}
            className={nodeClasses('node', { highlighted: hotNodes.has(node.id), detached: !reachable.has(node.id), selected })}
            {...selectableProps(node.id, node.value, selected, onSelect)}>
            <circle r={R} />
            <text className="node-value" y={6} textAnchor="middle">{node.value}</text>
            <text className="node-id" y={R + 13} textAnchor="middle">#{node.id}</text>
          </g>
        );
      })}

      <g className="marker" data-marker="ROOT">
        <text x={root.x} y={root.y - 52} textAnchor="middle">ROOT</text>
        <line x1={root.x} y1={root.y - 46} x2={root.x} y2={root.y - R - 4} markerEnd={arrows.normal} />
        {snapshot.root === null && <text className="null-text" x={root.x} y={root.y + 4} textAnchor="middle">NULL</text>}
      </g>

      <PointerLabels pointers={pointers} nodes={layout.nodes}
        spot={(at, line) => ({ x: at.x + R + 6, y: at.y - R + 6 + line * 13 })} />
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
