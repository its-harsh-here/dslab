import { useState, type FormEvent } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { ListSnapshot, NodeId } from '../engine/protocol';
import { chainFrom } from '../layout/layout';
import { listLayout } from '../layout/linear';
import { useStructure } from '../playback/useStructure';
import { ChainSvg } from '../render/ChainSvg';
import { INTEGER_HINT, IntegerField, readInteger, StepReport } from './common';

export function ListPanel({ engine }: { engine: LoadedEngine }) {
  const view = useStructure<ListSnapshot>(engine, 'list', listLayout);
  const [valueText, setValueText] = useState('');
  const [indexText, setIndexText] = useState('0');
  const [invalid, setInvalid] = useState<'value' | 'index' | null>(null);
  // The selected node is remembered by its stable ID, never by its value.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function handleInsert(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(valueText);
    const index = readInteger(indexText);
    if (value === null || index === null) {
      setInvalid(value === null ? 'value' : 'index');
      return;
    }
    setInvalid(null);
    view.send('insertAt', { index, value });
  }

  function handleSearch() {
    const value = readInteger(valueText);
    setInvalid(value === null ? 'value' : null);
    if (value !== null) view.send('search', { value });
  }

  function handleDelete() {
    if (selected) view.send('deleteNode', { nodeId: selected.id });
  }

  const { snapshot } = view.frame;
  const chain = chainFrom(snapshot.nodes, snapshot.head);
  const label =
    snapshot.size === 0 ? 'Empty linked list' : `Linked list with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <section className="structure-panel" aria-labelledby="list-heading">
      <h2 id="list-heading">Singly linked list</h2>
      <form className="controls" onSubmit={handleInsert} noValidate>
        <IntegerField id="list-value" label="Value" value={valueText} onChange={setValueText} invalid={invalid === 'value'} />
        <IntegerField id="list-index" label="Index" value={indexText} onChange={setIndexText} invalid={invalid === 'index'}
          placeholder="e.g. 0" />
        <button type="submit">Insert at index</button>
        <button type="button" onClick={handleSearch}>Search value</button>
        <button type="button" onClick={handleDelete} disabled={!selected}>Delete selected</button>
        <button type="button" onClick={() => view.send('reverse')}>Reverse</button>
        <button type="button" onClick={() => view.send('clear')}>Clear</button>
      </form>
      {invalid && <p className="input-error">{INTEGER_HINT} (check the {invalid} box)</p>}
      <p className="selection">
        {selected ? `Selected: node #${selected.id} (value ${selected.value}).` : 'Click a node to select it.'}
      </p>

      <div className="stage">
        <ChainSvg
          frame={view.frame}
          layout={view.layout}
          viewBox={view.animation.viewBox}
          entry={{ name: 'HEAD', target: snapshot.head }}
          exit={{ name: 'TAIL', target: chain.length > 0 ? chain[chain.length - 1] : null }}
          label={label}
          showIndices
          selectedId={selected?.id ?? null}
          onSelect={(id) => setSelectedId((current) => (current === id ? null : id))}
        />
      </div>
      <StepReport view={view} />
    </section>
  );
}
