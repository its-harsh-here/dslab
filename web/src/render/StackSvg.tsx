// Draws one moment of a stack animation: the frame C++ recorded, at the
// positions the layout computed. Purely presentational.
import { edgeId, type Frame, type StackSnapshot } from '../engine/protocol';
import { chainFrom, type Layout, type ViewBox } from '../layout/layout';
import { STACK_NODE } from '../layout/linear';
import { ArrowDefs, BoxNode, PointerLabels, useArrows } from './parts';

const { width: W, height: H } = STACK_NODE;

export function StackSvg({ frame, layout, viewBox }: { frame: Frame<StackSnapshot>; layout: Layout; viewBox: ViewBox }) {
  const arrows = useArrows();
  const { snapshot, highlight, pointers } = frame;
  const reachable = new Set(chainFrom(snapshot.nodes, snapshot.top));
  const hotNodes = new Set(highlight.nodes);
  const hotEdges = new Set(highlight.edges);
  const top = layout.markers.get('TOP') ?? { x: 0, y: 0 };
  const topNode = snapshot.nodes.find((node) => node.id === snapshot.top);

  const label =
    snapshot.size === 0
      ? 'Empty stack'
      : `Stack with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}` + (topNode ? `, top value ${topNode.value}` : '');

  return (
    <svg className="structure-svg" role="img" aria-label={label} width={viewBox.width} height={viewBox.height}
      viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}>
      <ArrowDefs arrows={arrows} />

      {/* next links first, so nodes sit on top of them */}
      {snapshot.nodes.map((node) => {
        const from = layout.nodes.get(node.id);
        if (!from) return null;
        const x1 = from.x + W / 2;
        const y1 = from.y + H;
        if (node.next === null) {
          return (
            <g key={`null-${node.id}`} className="null-link">
              <line x1={x1} y1={y1} x2={x1} y2={y1 + 16} markerEnd={arrows.normal} />
              <text x={x1} y={y1 + 32} textAnchor="middle">NULL</text>
            </g>
          );
        }
        const to = layout.nodes.get(node.next);
        if (!to) return null;
        const id = edgeId(node.id, node.next);
        const hot = hotEdges.has(id);
        return (
          <line key={id} data-edge-id={id} className={hot ? 'edge highlighted' : 'edge'}
            x1={x1} y1={y1} x2={to.x + W / 2} y2={to.y - 2} markerEnd={hot ? arrows.hot : arrows.normal} />
        );
      })}

      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        return at ? (
          <BoxNode key={node.id} id={node.id} value={node.value} at={at} width={W} height={H}
            highlighted={hotNodes.has(node.id)} detached={!reachable.has(node.id)} />
        ) : null;
      })}

      <g className="marker" transform={`translate(${top.x} ${top.y + H / 2})`}>
        <text x={-100} y={5}>TOP</text>
        <line x1={-62} y1={0} x2={-6} y2={0} markerEnd={arrows.normal} />
        {snapshot.top === null && <text className="null-text" x={4} y={5}>NULL</text>}
      </g>

      <PointerLabels pointers={pointers} nodes={layout.nodes} spot={(at, line) => ({ x: at.x + W + 10, y: at.y + H / 2 + 5 + line * 14 })} />
    </svg>
  );
}
