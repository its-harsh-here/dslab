// Draws one moment of the hash table: the bucket array down the left, and
// each bucket's chain running to the right of its cell. The bucket the
// current step works in (frame.bucket, set by C++) is lit. Purely presentational.
import { edgeId, type Frame, type HashTableSnapshot, type NodeId } from '../engine/protocol';
import type { Layout, ViewBox } from '../layout/layout';
import { bucketChains, HASH_CELL, HASH_FIRST_X, HASH_NODE, HASH_ROW } from '../layout/linear';
import { ArrowDefs, BoxNode, PointerLabels, useArrows } from './parts';

const { width: W, height: H } = HASH_NODE;

interface Props {
  frame: Frame<HashTableSnapshot>;
  layout: Layout;
  viewBox: ViewBox;
  label: string;
  selectedId?: NodeId | null;
  onSelect?: (id: NodeId) => void;
}

export function HashSvg({ frame, layout, viewBox, label, selectedId, onSelect }: Props) {
  const arrows = useArrows();
  const { snapshot, highlight, pointers } = frame;
  const chains = bucketChains(snapshot);
  const linked = new Set(chains.flat());
  const hotNodes = new Set(highlight.nodes);
  const hotEdges = new Set(highlight.edges);
  const active = frame.bucket ?? null;

  return (
    <svg className="structure-svg" role="group" aria-label={label} width="100%" height="100%"
      preserveAspectRatio="xMidYMid meet" viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}>
      <ArrowDefs arrows={arrows} />

      <text className="hash-title" x={HASH_CELL.width / 2} y={-18} textAnchor="middle">buckets</text>

      {snapshot.buckets.map((first, bucket) => {
        const y = bucket * HASH_ROW;
        const middle = y + HASH_CELL.height / 2;
        const target = first === null ? undefined : layout.nodes.get(first);
        const lit = active === bucket;
        return (
          <g key={bucket} className={lit ? 'bucket active' : 'bucket'} data-bucket={bucket}>
            <rect x={0} y={y} width={HASH_CELL.width} height={HASH_CELL.height} rx={8} />
            <text className="bucket-index" x={-12} y={middle + 5} textAnchor="end">{bucket}</text>
            <circle className="bucket-dot" cx={HASH_CELL.width - 18} cy={middle} r={4} />
            {first === null || !target ? (
              <g className="null-link">
                <line x1={HASH_CELL.width - 18} y1={middle} x2={HASH_CELL.width + 20} y2={middle} markerEnd={arrows.normal} />
                <text x={HASH_CELL.width + 25} y={middle + 4}>NULL</text>
              </g>
            ) : (
              <line className={lit ? 'bucket-link active' : 'bucket-link'} x1={HASH_CELL.width - 18} y1={middle}
                x2={target.x - 3} y2={target.y + H / 2} markerEnd={lit ? arrows.hot : arrows.normal} />
            )}
          </g>
        );
      })}

      {snapshot.nodes.map((node) => {
        const from = layout.nodes.get(node.id);
        if (!from) return null;
        const x1 = from.x + W;
        const y1 = from.y + H / 2;
        if (node.next === null) {
          return (
            <g key={`null-${node.id}`} className="null-link">
              <line x1={x1} y1={y1} x2={x1 + 18} y2={y1} markerEnd={arrows.normal} />
              <text x={x1 + 22} y={y1 + 4}>NULL</text>
            </g>
          );
        }
        const to = layout.nodes.get(node.next);
        if (!to) return null;
        const id = edgeId(node.id, node.next);
        const hot = hotEdges.has(id);
        return (
          <line key={id} data-edge-id={id} className={hot ? 'edge highlighted' : 'edge'}
            x1={x1} y1={y1} x2={to.x - 3} y2={to.y + H / 2} markerEnd={hot ? arrows.hot : arrows.normal} />
        );
      })}

      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        return at ? (
          <BoxNode key={node.id} id={node.id} value={node.value} at={at} width={W} height={H}
            highlighted={hotNodes.has(node.id)} detached={!linked.has(node.id)}
            selected={selectedId === node.id} onSelect={onSelect} />
        ) : null;
      })}

      <text className="hash-title" x={HASH_FIRST_X} y={-18}>chains (bucket = value mod {snapshot.buckets.length})</text>

      <PointerLabels pointers={pointers} nodes={layout.nodes} spot={(at, line) => ({ x: at.x, y: at.y - 7 - line * 13 })} />
    </svg>
  );
}
