// Draws a left-to-right chain of nodes linked by `next` (the queue, and the
// list later), with one marker above its first node and one below its last.
import { edgeId, type Frame, type LinearNode, type NodeId, type Snapshot } from '../engine/protocol';
import { chainFrom, type Layout, type ViewBox } from '../layout/layout';
import { CHAIN_NODE } from '../layout/linear';
import { ArrowDefs, BoxNode, PointerLabels, useArrows, type Arrows } from './parts';

const { width: W, height: H } = CHAIN_NODE;

/** A marker and the node it points at (null means NIL). */
export interface ChainMarker {
  readonly name: string;
  readonly target: NodeId | null;
}

interface Props {
  frame: Frame<Snapshot & { nodes: readonly LinearNode[] }>;
  layout: Layout;
  viewBox: ViewBox;
  /** The entry pointer (FRONT, HEAD): drawn above, and decides what is reachable. */
  entry: ChainMarker;
  /** The marker at the other end (REAR, TAIL): drawn below. */
  exit: ChainMarker;
  label: string;
  /** Show each reachable node's index under it (the list). */
  showIndices?: boolean;
  selectedId?: NodeId | null;
  onSelect?: (id: NodeId) => void;
}

export function ChainSvg({ frame, layout, viewBox, entry, exit, label, showIndices, selectedId, onSelect }: Props) {
  const arrows = useArrows();
  const { snapshot, highlight, pointers } = frame;
  const chain = chainFrom(snapshot.nodes, entry.target);
  const indexOf = new Map(chain.map((id, index) => [id, index]));
  const hotNodes = new Set(highlight.nodes);
  const hotEdges = new Set(highlight.edges);
  const exitDrop = showIndices ? 34 : 20; // room for the index labels

  return (
    <svg className="structure-svg" role="group" aria-label={label} width="100%" height="100%"
      preserveAspectRatio="xMidYMid meet" viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}>
      <ArrowDefs arrows={arrows} />

      {snapshot.nodes.map((node) => {
        const from = layout.nodes.get(node.id);
        if (!from) return null;
        const x1 = from.x + W;
        const y1 = from.y + H / 2;
        if (node.next === null) {
          return (
            <g key={`null-${node.id}`} className="null-link">
              <line x1={x1} y1={y1} x2={x1 + 22} y2={y1} markerEnd={arrows.normal} />
              <text x={x1 + 26} y={y1 + 4}>NULL</text>
            </g>
          );
        }
        const to = layout.nodes.get(node.next);
        if (!to) return null;
        const id = edgeId(node.id, node.next);
        const hot = hotEdges.has(id);
        return (
          <line key={id} data-edge-id={id} className={hot ? 'edge highlighted' : 'edge'}
            x1={x1} y1={y1} x2={to.x - 2} y2={to.y + H / 2} markerEnd={hot ? arrows.hot : arrows.normal} />
        );
      })}

      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        if (!at) return null;
        const index = indexOf.get(node.id);
        return (
          <g key={node.id}>
            <BoxNode id={node.id} value={node.value} at={at} width={W} height={H}
              highlighted={hotNodes.has(node.id)} detached={index === undefined}
              selected={selectedId === node.id} onSelect={onSelect} />
            {showIndices && index !== undefined && (
              <text className="index-label" x={at.x + W / 2} y={at.y + H + 15} textAnchor="middle">[{index}]</text>
            )}
          </g>
        );
      })}

      <MarkerAbove name={entry.name} at={layout.markers.get(entry.name)} isNull={entry.target === null} arrows={arrows} />
      <MarkerBelow name={exit.name} at={layout.markers.get(exit.name)} drop={exitDrop} arrows={arrows} />

      <PointerLabels pointers={pointers} nodes={layout.nodes} spot={(at, line) => ({ x: at.x, y: at.y - 8 - line * 14 })} />
    </svg>
  );
}

interface MarkerProps {
  name: string;
  at?: { x: number; y: number };
  arrows: Arrows;
}

function MarkerAbove({ name, at, isNull, arrows }: MarkerProps & { isNull: boolean }) {
  if (!at) return null;
  const x = at.x + W / 2;
  return (
    <g className="marker" data-marker={name}>
      <text x={x} y={at.y - 34} textAnchor="middle">{name}</text>
      <line x1={x} y1={at.y - 28} x2={x} y2={at.y - 4} markerEnd={arrows.normal} />
      {isNull && <text className="null-text" x={x} y={at.y + H / 2 + 4} textAnchor="middle">NULL</text>}
    </g>
  );
}

// The bottom marker is NIL only when the structure is empty, and then the top
// marker already shows NULL in the same place.
function MarkerBelow({ name, at, drop, arrows }: MarkerProps & { drop: number }) {
  if (!at) return null;
  const x = at.x + W / 2;
  const bottom = at.y + H;
  return (
    <g className="marker" data-marker={name}>
      <text x={x} y={bottom + drop + 28} textAnchor="middle">{name}</text>
      <line x1={x} y1={bottom + drop + 14} x2={x} y2={bottom + drop - 16} markerEnd={arrows.normal} />
    </g>
  );
}
