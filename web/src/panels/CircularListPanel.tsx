import { useState } from 'react';

import type { CircularListSnapshot, NodeId } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { RingSvg } from '../render/RingSvg';
import { OpButton } from '../ui/OperationBar';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, randomValues, setUp } from './examples';

export function CircularListPanel({ player, catalog, stageCommands }: PanelProps<CircularListSnapshot>) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  // The selected node is remembered by its stable ID, never by its value.
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);
  const selected = view.state.latest.nodes.find((node) => node.id === selectedId) ?? null;

  function withValue(operation: 'insertFront' | 'insertBack' | 'search') {
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null && player.run(operation, { value }) && operation !== 'search') setInput('');
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? 'Empty circular list' : `Circular list with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <Workspace title="Circular linked list" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId}
      hint={selected ? `Selected: node #${selected.id} (value ${selected.value}).`
        : 'The last node links back to the first; only tail is stored. Click a node to select it.'}
      controls={
        <>
          <form className="controls" onSubmit={(event) => { event.preventDefault(); withValue('insertBack'); }} noValidate>
            <IntegerField id="circularlist-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton onClick={() => withValue('insertFront')}>Insert front</OpButton>
            <OpButton type="submit">Insert back</OpButton>
            <OpButton onClick={() => withValue('search')}>Search value</OpButton>
            <OpButton onClick={() => selected && player.run('deleteNode', { nodeId: selected.id })} disabled={!selected}>
              Delete selected
            </OpButton>
            <OpButton onClick={() => player.run('rotate')}>Rotate</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Insert back all', step: (value) => animated('insertBack', value) },
        random: (random, count) => setUp('insertBack', randomValues(random, count)),
        presets: [
          {
            name: 'Round robin: rotate three times',
            steps: [
              ...setUp<CircularListSnapshot>('insertBack', [10, 20, 30, 40, 50]),
              { operation: 'rotate', animate: true },
              { operation: 'rotate', animate: true },
              { operation: 'rotate', animate: true },
            ],
          },
          {
            name: 'A search that goes all the way round',
            steps: [...setUp<CircularListSnapshot>('insertBack', [4, 8, 15, 16, 23, 42]), animated<CircularListSnapshot>('search', 99)],
          },
        ],
      }}
      drawing={(viewBox) => (
        <RingSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label}
          selectedId={selectedId} onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
