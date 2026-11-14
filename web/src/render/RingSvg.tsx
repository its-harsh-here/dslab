// Draws one moment of a circular list as a ring: the first node (tail.next) at
// the top, the others clockwise, every next link an arrow to the neighbour,
// and the last link closing the circle. Purely presentational.
import { edgeId, type CircularListSnapshot, type Frame, type NodeId } from '../engine/protocol';
import type { Layout, Point, ViewBox } from '../layout/layout';
import { CHAIN_NODE, circleFrom } from '../layout/linear';
import { ArrowDefs, BoxNode, PointerLabels, useArrows } from './parts';

const { width: W, height: H } = CHAIN_NODE;

interface Props {
  frame: Frame<CircularListSnapshot>;
  layout: Layout;
  viewBox: ViewBox;
  label: string;
  selectedId?: NodeId | null;
  onSelect?: (id: NodeId) => void;
}

/** Where the segment from the centre of box `a` towards `target` leaves the box (plus `gap`). */
function boxEdge(a: Point, target: Point, gap: number): Point {
  const cx = a.x + W / 2;
  const cy = a.y + H / 2;
  const dx = target.x - cx;
  const dy = target.y - cy;
  const length = Math.hypot(dx, dy) || 1;
  // The distance from the centre to the box's border along this direction.
  const reach = Math.min(Math.abs(dx) < 1e-6 ? Infinity : W / 2 / Math.abs(dx / length), Math.abs(dy) < 1e-6 ? Infinity : H / 2 / Math.abs(dy / length));
  return { x: cx + (dx / length) * (reach + gap), y: cy + (dy / length) * (reach + gap) };
}

export function RingSvg({ frame, layout, viewBox, label, selectedId, onSelect }: Props) {
  const arrows = useArrows();
  const { snapshot, highlight, pointers } = frame;
  const order = circleFrom(snapshot);
  const indexOf = new Map(order.map((id, index) => [id, index]));
  const hotNodes = new Set(highlight.nodes);
  const hotEdges = new Set(highlight.edges);
  const centre = (at: Point): Point => ({ x: at.x + W / 2, y: at.y + H / 2 });
  const tailAt = layout.markers.get('TAIL');
  const first = order.length > 0 ? layout.nodes.get(order[0]) : undefined;

  /** A label pushed outwards from the ring's middle, past the node at `at`. */
  const outside = (at: Point, distance: number): Point => {
    const c = centre(at);
    const length = Math.hypot(c.x, c.y);
    if (length < 1) return { x: c.x, y: c.y + H / 2 + distance }; // a single node in the middle: below it
    return { x: c.x + (c.x / length) * (W / 2 + distance), y: c.y + (c.y / length) * (H / 2 + distance) };
  };

  return (
    <svg className="structure-svg" role="group" aria-label={label} width="100%" height="100%"
      preserveAspectRatio="xMidYMid meet" viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}>
      <ArrowDefs arrows={arrows} />

      {snapshot.nodes.map((node) => {
        const from = layout.nodes.get(node.id);
        if (!from || node.next === null) return null;
        const to = layout.nodes.get(node.next);
        if (!to) return null;
        const id = edgeId(node.id, node.next);
        const hot = hotEdges.has(id);
        const className = hot ? 'edge highlighted' : 'edge';
        const marker = hot ? arrows.hot : arrows.normal;
        if (node.next === node.id || (Math.abs(from.x - to.x) < 1 && Math.abs(from.y - to.y) < 1)) {
          // A link back to the node itself (a circle of one): a loop above the box.
          const x = from.x + W / 2;
          return (
            <path key={id} data-edge-id={id} className={className} fill="none" markerEnd={marker}
              d={`M ${x + 18} ${from.y} C ${x + 46} ${from.y - 50}, ${x - 46} ${from.y - 50}, ${x - 18} ${from.y - 3}`} />
          );
        }
        const start = boxEdge(from, centre(to), 0);
        const end = boxEdge(to, centre(from), 3);
        return (
          <line key={id} data-edge-id={id} className={className} x1={start.x} y1={start.y} x2={end.x} y2={end.y} markerEnd={marker} />
        );
      })}

      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        if (!at) return null;
        const index = indexOf.get(node.id);
        const indexAt = outside(at, 14);
        return (
          <g key={node.id}>
            <BoxNode id={node.id} value={node.value} at={at} width={W} height={H}
              highlighted={hotNodes.has(node.id)} detached={index === undefined || node.next === null}
              selected={selectedId === node.id} onSelect={onSelect} />
            {index !== undefined && node.id !== snapshot.tail && index !== 0 && (
              <text className="index-label" x={indexAt.x} y={indexAt.y + 4} textAnchor="middle">[{index}]</text>
            )}
          </g>
        );
      })}

      {tailAt && snapshot.tail !== null && (
        <g className="marker" data-marker="TAIL">
          <text x={outside(tailAt, 26).x} y={outside(tailAt, 26).y + 5} textAnchor="middle">TAIL</text>
        </g>
      )}
      {first && order.length > 1 && (
        <g className="marker soft" data-marker="FIRST">
          <text x={outside(first, 22).x} y={outside(first, 22).y + 5} textAnchor="middle">first [0]</text>
        </g>
      )}
      {snapshot.tail === null && (
        <g className="marker" data-marker="TAIL">
          <text x={0} y={-34} textAnchor="middle">TAIL</text>
          <line x1={0} y1={-28} x2={0} y2={-8} markerEnd={arrows.normal} />
          <text className="null-text" x={0} y={8} textAnchor="middle">NULL</text>
        </g>
      )}

      <PointerLabels pointers={pointers} nodes={layout.nodes} spot={(at, line) => ({ x: at.x, y: at.y - 8 - line * 14 })} />
    </svg>
  );
}
