import { useState, type FormEvent } from 'react';

import type { NodeId, QueueSnapshot } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { ChainSvg } from '../render/ChainSvg';
import { OpButton } from '../ui/OperationBar';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, randomValues, setUp } from './examples';

export function QueuePanel({ player, catalog, stageCommands }: PanelProps<QueueSnapshot>) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);

  function handleEnqueue(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null && player.run('enqueue', { value })) setInput('');
  }

  const { snapshot } = view.frame;
  const label = snapshot.size === 0 ? 'Empty queue' : `Queue with ${snapshot.size} node${snapshot.size === 1 ? '' : 's'}`;

  return (
    <Workspace title="Queue" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId} hint="Click a node to see its properties."
      controls={
        <>
          <form className="controls" onSubmit={handleEnqueue} noValidate>
            <IntegerField id="queue-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton type="submit">Enqueue</OpButton>
            <OpButton onClick={() => player.run('dequeue')}>Dequeue</OpButton>
            <OpButton onClick={() => player.run('front')}>Front</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Enqueue all', step: (value) => animated('enqueue', value) },
        random: (random, count) => setUp('enqueue', randomValues(random, count)),
      }}
      drawing={(viewBox) => (
        <ChainSvg frame={view.frame} layout={view.layout} viewBox={viewBox} label={label}
          entry={{ name: 'FRONT', target: snapshot.front }} exit={{ name: 'REAR', target: snapshot.rear }}
          selectedId={selectedId} onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
