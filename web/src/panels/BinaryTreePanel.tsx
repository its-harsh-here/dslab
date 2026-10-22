import { useState, type FormEvent } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { NodeId, TreeSnapshot } from '../engine/protocol';
import { treeLayout } from '../layout/tree';
import { useStructure } from '../playback/useStructure';
import { TreeSvg } from '../render/TreeSvg';
import { INTEGER_HINT, IntegerField, readInteger, StepReport } from './common';
import { TraversalButtons } from './TraversalButtons';

export function BinaryTreePanel({ engine }: { engine: LoadedEngine }) {
  const view = useStructure<TreeSnapshot>(engine, 'binaryTree', treeLayout);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  // The selected node is remembered by its stable ID, never by its value.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function readValue(): number | null {
    const value = readInteger(input);
    setInputError(value === null);
    return value;
  }

  function handleAddRoot(event: FormEvent) {
    event.preventDefault();
    const value = readValue();
    if (value !== null) view.send('addRoot', { value });
  }

  function handleAddChild(side: 'left' | 'right') {
    const value = readValue();
    if (value !== null && selected) view.send('addChild', { parentId: selected.id, side, value });
  }

  function handleDeleteSubtree() {
    if (selected) view.send('deleteSubtree', { nodeId: selected.id });
  }

  const { snapshot } = view.frame;
  const label =
    snapshot.size === 0 ? 'Empty binary tree' : `Binary tree with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <section className="structure-panel" aria-labelledby="binary-tree-heading">
      <h2 id="binary-tree-heading">Binary tree</h2>
      <form className="controls" onSubmit={handleAddRoot} noValidate>
        <IntegerField id="tree-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
        <button type="submit">Add root</button>
        <button type="button" onClick={() => handleAddChild('left')} disabled={!selected}>Add left child</button>
        <button type="button" onClick={() => handleAddChild('right')} disabled={!selected}>Add right child</button>
        <button type="button" onClick={handleDeleteSubtree} disabled={!selected}>Delete selected subtree</button>
        <button type="button" onClick={() => view.send('clear')}>Clear</button>
      </form>
      <TraversalButtons send={view.send} />
      {inputError && <p className="input-error">{INTEGER_HINT}</p>}
      <p className="selection">
        {selected
          ? `Selected: node #${selected.id} (value ${selected.value}). New children go under it.`
          : 'Click a node to select it; new children go under the selected node.'}
      </p>

      <div className="stage">
        <TreeSvg frame={view.frame} layout={view.layout} viewBox={view.animation.viewBox} label={label}
          selectedId={selected?.id ?? null}
          onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      </div>
      <StepReport view={view} />
    </section>
  );
}
