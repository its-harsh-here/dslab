// The right-hand column: what the frame on screen says, where it is in the
// pseudocode, the selected node, the work counted so far, and the structure
// as text. Everything here only reads the frame and what C++ returned.
import { useLayoutEffect, useRef } from 'react';

import type {
  CatalogOperation,
  Counters,
  Frame,
  LinearNode,
  NodeId,
  Snapshot,
  StructureName,
  TreeNode,
  TreeSnapshot,
} from '../engine/protocol';
import { chainFrom } from '../layout/layout';
import type { PlayerView } from '../playback/usePlayer';
import { describeResult, eventLabel } from './describe';

// --- Explanation -------------------------------------------------------------

export function ExplanationPanel<S extends Snapshot>({ view }: { view: PlayerView<S> }) {
  const { state, frame, busy } = view;
  const valueOf = new Map<NodeId, number>(frame.snapshot.nodes.map((node) => [node.id, node.value]));
  // The result is shown once the playback reaches it; a protocol error (no frames) at once.
  const result = state.lastResult;
  const showResult = result !== null && (!busy || state.lastResult !== state.response.result);
  return (
    <section className="panel" aria-labelledby="explanation-heading">
      <h3 id="explanation-heading">Explanation</h3>
      <p className="explanation" aria-live="polite">{frame.explanation}</p>
      <p className="meta">
        Step {state.index + 1} of {state.animation.frames.length} · {eventLabel(frame.event)}
      </p>
      {frame.output.length > 0 && (
        <div className="output">
          <span className="label">Output so far</span>
          <ol className="value-list">
            {frame.output.map((value, index) => <li key={index}>{value}</li>)}
          </ol>
        </div>
      )}
      {frame.pending.length > 0 && (
        <div className="output">
          <span className="label">Queue, front first</span>
          <ol className="value-list">
            {frame.pending.map((id) => <li key={id}>{valueOf.get(id) ?? `#${id}`}</li>)}
          </ol>
        </div>
      )}
      {showResult && result && (
        <p className={result.status === 'error' ? 'result error' : 'result'} role={result.status === 'error' ? 'alert' : undefined}>
          <span className="label">Result of {state.lastOperation}</span> {describeResult(state.lastOperation, result)}
        </p>
      )}
    </section>
  );
}

// --- Pseudocode --------------------------------------------------------------

export function PseudocodePanel({ catalog, operation, stepId }: {
  catalog: readonly CatalogOperation[];
  operation: string;
  stepId: string;
}) {
  const listing = catalog.find((entry) => entry.operation === operation);
  const box = useRef<HTMLOListElement>(null);

  // Keep the current line in view inside the panel (never scroll the page).
  useLayoutEffect(() => {
    const list = box.current;
    const line = list?.querySelector<HTMLElement>('[aria-current="step"]');
    if (!list || !line) return;
    const top = line.offsetTop - list.offsetTop;
    if (top < list.scrollTop || top + line.offsetHeight > list.scrollTop + list.clientHeight) {
      list.scrollTop = Math.max(0, top - list.clientHeight / 3);
    }
  }, [stepId, operation]);

  return (
    <section className="panel" aria-labelledby="pseudocode-heading">
      <h3 id="pseudocode-heading">
        Pseudocode{listing ? `: ${listing.operation}` : ''}
      </h3>
      {listing ? (
        <ol ref={box} className="pseudocode" aria-label={`Pseudocode of ${listing.operation}`}>
          {listing.pseudocode.map((line, index) => {
            const current = line.stepId !== null && line.stepId === stepId;
            // Leading spaces become a left padding, so a wrapped line stays under its own indentation.
            const text = line.text.trimStart();
            const indent = line.text.length - text.length;
            return (
              <li key={index} aria-current={current ? 'step' : undefined} className={current ? 'current' : undefined}
                style={{ paddingLeft: `calc(8px + ${indent}ch)` }}>
                <code>{text}</code>
              </li>
            );
          })}
        </ol>
      ) : (
        <p className="meta">
          {['undo', 'redo', 'import', 'export'].includes(operation)
            ? `${operation} works on a whole saved copy of the structure at once, so it has no step-by-step pseudocode.`
            : 'Run an operation to see its pseudocode here.'}
        </p>
      )}
    </section>
  );
}

// --- Node properties ---------------------------------------------------------

function isTree(snapshot: Snapshot): snapshot is TreeSnapshot {
  return 'root' in snapshot;
}

function nodeName(snapshot: Snapshot, id: NodeId | null): string {
  if (id === null) return 'NIL';
  const node = snapshot.nodes.find((candidate) => candidate.id === id);
  return node ? `${node.value} (#${id})` : `#${id}`;
}

export function NodeProperties({ snapshot, selectedId }: { snapshot: Snapshot; selectedId: NodeId | null }) {
  const node = selectedId === null ? undefined : snapshot.nodes.find((candidate) => candidate.id === selectedId);
  const rows: [string, string][] = [];
  if (node) {
    rows.push(['ID', `#${node.id}`], ['Value', String(node.value)]);
    if (isTree(snapshot)) {
      const tree = node as TreeNode;
      if (tree.color) rows.push(['Color', tree.color]);
      if (tree.height !== undefined) rows.push(['Height', String(tree.height)]);
      if (tree.balance !== undefined) rows.push(['Balance', tree.balance > 0 ? `+${tree.balance}` : String(tree.balance)]);
      rows.push(['Left child', nodeName(snapshot, tree.left)], ['Right child', nodeName(snapshot, tree.right)]);
      rows.push(['Parent', tree.parent === null ? (snapshot.root === tree.id ? 'none (the root)' : 'none') : nodeName(snapshot, tree.parent)]);
    } else {
      const linear = node as LinearNode;
      const entry = 'top' in snapshot ? snapshot.top : 'front' in snapshot ? snapshot.front : snapshot.head;
      const index = chainFrom(snapshot.nodes as readonly LinearNode[], entry).indexOf(node.id);
      rows.push(['Next', nodeName(snapshot, linear.next)]);
      rows.push(['Position', index >= 0 ? `${index} from the ${'top' in snapshot ? 'top' : 'front' in snapshot ? 'front' : 'head'}` : 'not linked in this step']);
    }
  }
  return (
    <section className="panel" aria-labelledby="node-heading">
      <h3 id="node-heading">Selected node</h3>
      {selectedId === null && <p className="meta">Click a node (or focus it and press Enter) to see its properties.</p>}
      {selectedId !== null && !node && <p className="meta">Node #{selectedId} does not exist in this step.</p>}
      {node && (
        <dl className="properties">
          {rows.map(([name, value]) => (
            <div key={name}>
              <dt>{name}</dt>
              <dd>{value}</dd>
            </div>
          ))}
        </dl>
      )}
    </section>
  );
}

// --- Counters and complexity -------------------------------------------------

const COUNTER_NAMES: [keyof Counters, string][] = [
  ['comparisons', 'Comparisons'],
  ['nodesVisited', 'Nodes visited'],
  ['linksChanged', 'Links changed'],
  ['rotations', 'Rotations'],
  ['recolors', 'Recolors'],
];

export function CostPanel({ structure, frame, total, frames, snapshotNodes, complexity }: {
  structure: StructureName;
  frame: Frame;
  total: Counters;
  frames: number;
  snapshotNodes: number;
  complexity: string | null;
}) {
  const shown = COUNTER_NAMES.filter(([key]) =>
    (key !== 'rotations' || structure === 'avl' || structure === 'redBlack') && (key !== 'recolors' || structure === 'redBlack'));
  return (
    <section className="panel" aria-labelledby="cost-heading">
      <h3 id="cost-heading">Cost</h3>
      <table className="counters">
        <caption>Algorithm work, counted by C++</caption>
        <thead>
          <tr>
            <th scope="col">Counter</th>
            <th scope="col">So far</th>
            <th scope="col">Whole operation</th>
          </tr>
        </thead>
        <tbody>
          {shown.map(([key, name]) => (
            <tr key={key}>
              <th scope="row">{name}</th>
              <td>{frame.counters[key]}</td>
              <td>{total[key]}</td>
            </tr>
          ))}
        </tbody>
      </table>
      <p className="complexity">
        <span className="label">Theoretical complexity</span> {complexity ?? '—'}
      </p>
      <p className="meta">
        Tracing cost (not part of the algorithm): {frames} frames, {snapshotNodes} node copies.
      </p>
    </section>
  );
}

// --- Text view ---------------------------------------------------------------

function treeLabel(node: TreeNode): string {
  const extra = node.color ?? (node.height !== undefined ? `h${node.height}, bf ${node.balance! > 0 ? '+' : ''}${node.balance}` : null);
  return `${node.value} (#${node.id}${extra ? `, ${extra}` : ''})`;
}

function TreeItem({ id, byId, side }: { id: NodeId | null; byId: Map<NodeId, TreeNode>; side: string }) {
  const node = id === null ? undefined : byId.get(id);
  if (!node) return <li>{side}NIL</li>;
  const leaf = node.left === null && node.right === null;
  return (
    <li>
      {side}
      {treeLabel(node)}
      {!leaf && (
        <ul>
          <TreeItem id={node.left} byId={byId} side="L: " />
          <TreeItem id={node.right} byId={byId} side="R: " />
        </ul>
      )}
    </li>
  );
}

export function TextView({ snapshot }: { snapshot: Snapshot }) {
  let body;
  if (isTree(snapshot)) {
    const byId = new Map(snapshot.nodes.map((node) => [node.id, node]));
    body = snapshot.root === null ? <p>The tree is empty (root is NIL).</p> : <ul className="text-tree"><TreeItem id={snapshot.root} byId={byId} side="root: " /></ul>;
  } else {
    const entryName = 'top' in snapshot ? 'top' : 'front' in snapshot ? 'front' : 'head';
    const entry = 'top' in snapshot ? snapshot.top : 'front' in snapshot ? snapshot.front : snapshot.head;
    const nodes = snapshot.nodes as readonly LinearNode[];
    const chain = chainFrom(nodes, entry);
    const byId = new Map(nodes.map((node) => [node.id, node]));
    const loose = nodes.filter((node) => !chain.includes(node.id));
    body = (
      <>
        <p>From {entryName}, following next:</p>
        {chain.length === 0 ? <p>NULL (empty)</p> : (
          <ol className="text-chain">
            {chain.map((id) => <li key={id}>{byId.get(id)!.value} (#{id})</li>)}
          </ol>
        )}
        {loose.length > 0 && <p>Not linked in this step: {loose.map((node) => `${node.value} (#${node.id})`).join(', ')}.</p>}
      </>
    );
  }
  return (
    <details className="panel text-view">
      <summary>Text view of this step</summary>
      {body}
    </details>
  );
}
