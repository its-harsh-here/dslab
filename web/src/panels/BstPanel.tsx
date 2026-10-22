import { useState, type FormEvent } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { NodeId, TreeSnapshot } from '../engine/protocol';
import { treeLayout } from '../layout/tree';
import { useStructure } from '../playback/useStructure';
import { TreeSvg } from '../render/TreeSvg';
import { INTEGER_HINT, IntegerField, readInteger, StepReport } from './common';
import { TraversalButtons } from './TraversalButtons';

export function BstPanel({ engine }: { engine: LoadedEngine }) {
  const view = useStructure<TreeSnapshot>(engine, 'bst', treeLayout);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  // Selection is by stable ID. BST delete and search are keyed by value
  // (decision D12), so selecting a node copies its value into the box.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function sendWithValue(operation: 'insert' | 'delete' | 'search') {
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null) view.send(operation, { value });
  }

  function handleInsert(event: FormEvent) {
    event.preventDefault();
    sendWithValue('insert');
  }

  function handleSelect(id: NodeId) {
    const next = selectedId === id ? null : id;
    setSelectedId(next);
    const node = view.latest.nodes.find((candidate) => candidate.id === next);
    if (node) setInput(String(node.value));
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? 'Empty BST' : `BST with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <section className="structure-panel" aria-labelledby="bst-heading">
      <h2 id="bst-heading">Binary search tree</h2>
      <form className="controls" onSubmit={handleInsert} noValidate>
        <IntegerField id="bst-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
        <button type="submit">Insert</button>
        <button type="button" onClick={() => sendWithValue('delete')}>Delete</button>
        <button type="button" onClick={() => sendWithValue('search')}>Search</button>
        <button type="button" onClick={() => view.send('clear')}>Clear</button>
      </form>
      <TraversalButtons send={view.send} />
      {inputError && <p className="input-error">{INTEGER_HINT}</p>}
      <p className="selection">
        {selected
          ? `Selected: node #${selected.id} (value ${selected.value}); its value is in the box for Delete or Search.`
          : 'Click a node to put its value in the box.'}
      </p>

      <div className="stage">
        <TreeSvg frame={view.frame} layout={view.layout} viewBox={view.animation.viewBox} label={label}
          selectedId={selected?.id ?? null} onSelect={handleSelect} />
      </div>
      <StepReport view={view} />
    </section>
  );
}
