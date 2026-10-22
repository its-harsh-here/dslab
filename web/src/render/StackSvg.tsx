// Draws one moment of an animation: the frame C++ recorded, at the positions
// the layout computed. Purely presentational.
import { edgeId, type Frame } from '../engine/protocol';
import { chainFromTop, NODE_HEIGHT, NODE_WIDTH, type StackLayout } from '../layout/stackLayout';
import type { Animation } from '../playback/animation';

interface Props {
  frame: Frame;
  layout: StackLayout;
  viewBox: Animation['viewBox'];
}

export function StackSvg({ frame, layout, viewBox }: Props) {
  const { snapshot, highlight, pointers } = frame;
  const reachable = new Set(chainFromTop(frame));
  const highlightedNodes = new Set(highlight.nodes);
  const highlightedEdges = new Set(highlight.edges);
  const topNode = snapshot.nodes.find((node) => node.id === snapshot.top);

  const label =
    snapshot.size === 0
      ? 'Empty stack'
      : `Stack with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}` +
        (topNode ? `, top value ${topNode.value}` : '');

  return (
    <svg
      className="stack-svg"
      role="img"
      aria-label={label}
      width={viewBox.width}
      height={viewBox.height}
      viewBox={`${viewBox.x} ${viewBox.y} ${viewBox.width} ${viewBox.height}`}
    >
      <defs>
        <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
          <path d="M 0 0 L 10 5 L 0 10 z" className="arrow-head" />
        </marker>
      </defs>

      {/* next links, drawn first so nodes sit on top of them */}
      {snapshot.nodes.map((node) => {
        const from = layout.nodes.get(node.id);
        if (!from) return null;
        const x1 = from.x + NODE_WIDTH / 2;
        const y1 = from.y + NODE_HEIGHT;
        if (node.next === null) {
          return (
            <g key={`null-${node.id}`} className="null-link">
              <line x1={x1} y1={y1} x2={x1} y2={y1 + 16} markerEnd="url(#arrow)" />
              <text x={x1} y={y1 + 32} textAnchor="middle">NULL</text>
            </g>
          );
        }
        const to = layout.nodes.get(node.next);
        if (!to) return null;
        const id = edgeId(node.id, node.next);
        return (
          <line
            key={id}
            data-edge-id={id}
            className={highlightedEdges.has(id) ? 'edge highlighted' : 'edge'}
            x1={x1}
            y1={y1}
            x2={to.x + NODE_WIDTH / 2}
            y2={to.y - 2}
            markerEnd="url(#arrow)"
          />
        );
      })}

      {/* nodes, keyed by their stable C++ ID */}
      {snapshot.nodes.map((node) => {
        const at = layout.nodes.get(node.id);
        if (!at) return null;
        const classes = ['node'];
        if (highlightedNodes.has(node.id)) classes.push('highlighted');
        if (!reachable.has(node.id)) classes.push('detached');
        return (
          <g key={node.id} data-node-id={node.id} className={classes.join(' ')} transform={`translate(${at.x} ${at.y})`}>
            <rect width={NODE_WIDTH} height={NODE_HEIGHT} rx={6} />
            <text className="node-id" x={8} y={14}>#{node.id}</text>
            <text className="node-value" x={NODE_WIDTH / 2} y={NODE_HEIGHT / 2 + 6} textAnchor="middle">
              {node.value}
            </text>
          </g>
        );
      })}

      {/* the TOP marker */}
      <g className="top-marker" transform={`translate(${layout.top.x} ${layout.top.y + NODE_HEIGHT / 2})`}>
        <text x={-100} y={5}>TOP</text>
        <line x1={-62} y1={0} x2={-6} y2={0} markerEnd="url(#arrow)" />
        {snapshot.top === null && <text className="null-text" x={4} y={5}>NULL</text>}
      </g>

      {/* the algorithm's own variables, such as "node" */}
      {Object.entries(pointers).map(([name, id], index) => {
        const at = id === null ? undefined : layout.nodes.get(id);
        if (!at) return null;
        return (
          <text key={name} className="pointer-label" x={at.x + NODE_WIDTH + 10} y={at.y + NODE_HEIGHT / 2 + 5 + index * 14}>
            ← {name}
          </text>
        );
      })}
    </svg>
  );
}
