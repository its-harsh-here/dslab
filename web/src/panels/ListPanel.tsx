import { useState, type FormEvent } from 'react';

import type { ListSnapshot, NodeId } from '../engine/protocol';
import { chainFrom } from '../layout/layout';
import type { Step } from '../playback/player';
import { usePlayer } from '../playback/usePlayer';
import { ChainSvg } from '../render/ChainSvg';
import { OpButton } from '../ui/OperationBar';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { randomValues } from './examples';

/** Append = insert at index size. The size is read from C++'s state when the request is sent. */
function append(value: number, animate: boolean): Step<ListSnapshot> {
  return { operation: 'insertAt', args: (latest) => ({ index: latest.size, value }), animate };
}

export function ListPanel({ player, catalog, stageCommands }: PanelProps<ListSnapshot>) {
  const view = usePlayer(player);
  const [valueText, setValueText] = useState('');
  const [indexText, setIndexText] = useState('0');
  const [invalid, setInvalid] = useState<'value' | 'index' | null>(null);
  // The selected node is remembered by its stable ID, never by its value.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.state.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function handleInsert(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(valueText);
    const index = readInteger(indexText);
    if (value === null || index === null) {
      setInvalid(value === null ? 'value' : 'index');
      return;
    }
    setInvalid(null);
    player.run('insertAt', { index, value });
  }

  function handleSearch() {
    const value = readInteger(valueText);
    setInvalid(value === null ? 'value' : null);
    if (value !== null) player.run('search', { value });
  }

  const { snapshot } = view.frame;
  const chain = chainFrom(snapshot.nodes, snapshot.head);
  const label = snapshot.size === 0 ? 'Empty linked list' : `Linked list with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <Workspace title="Singly linked list" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId}
      hint={selected ? `Selected: node #${selected.id} (value ${selected.value}).` : 'Click a node to select it for Delete selected.'}
      controls={
        <>
          <form className="controls" onSubmit={handleInsert} noValidate>
            <IntegerField id="list-value" label="Value" value={valueText} onChange={setValueText} invalid={invalid === 'value'} />
            <IntegerField id="list-index" label="Index" value={indexText} onChange={setIndexText} invalid={invalid === 'index'}
              placeholder="e.g. 0" />
            <OpButton type="submit">Insert at index</OpButton>
            <OpButton onClick={handleSearch}>Search value</OpButton>
            <OpButton onClick={() => selected && player.run('deleteNode', { nodeId: selected.id })} disabled={!selected}>
              Delete selected
            </OpButton>
            <OpButton onClick={() => player.run('reverse')}>Reverse</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {invalid && <p className="input-error">{INTEGER_HINT} (check the {invalid} box)</p>}
        </>
      }
      examples={{
        batch: { button: 'Append all', step: (value) => append(value, true) },
        random: (random, count) => [
          { operation: 'clear', animate: false },
          ...randomValues(random, count).map((value) => append(value, false)),
        ],
      }}
      drawing={(viewBox) => (
        <ChainSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label} showIndices
          entry={{ name: 'HEAD', target: snapshot.head }}
          exit={{ name: 'TAIL', target: chain.length > 0 ? chain[chain.length - 1] : null }}
          selectedId={selectedId} onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
