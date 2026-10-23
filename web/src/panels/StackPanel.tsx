import { useState, type FormEvent } from 'react';

import type { NodeId, StackSnapshot } from '../engine/protocol';
import { usePlayer } from '../playback/usePlayer';
import { StackSvg } from '../render/StackSvg';
import { OpButton } from '../ui/OperationBar';
import { Workspace, type PanelProps } from '../ui/Workspace';
import { INTEGER_HINT, IntegerField, readInteger } from './common';
import { animated, randomValues, setUp } from './examples';

export function StackPanel({ player, catalog, stageCommands }: PanelProps<StackSnapshot>) {
  const view = usePlayer(player);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);
  const [selectedId, setSelectedId] = useState<NodeId | null>(null);

  // Engine calls happen only in event handlers, through the player
  // (docs/architecture.md, 2.7).
  function handlePush(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(input);
    setInputError(value === null);
    if (value !== null && player.run('push', { value })) setInput('');
  }

  return (
    <Workspace title="Stack" view={view} player={player} catalog={catalog} stageCommands={stageCommands}
      selectedId={selectedId} hint="Click a node to see its properties."
      controls={
        <>
          <form className="controls" onSubmit={handlePush} noValidate>
            <IntegerField id="stack-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
            <OpButton type="submit">Push</OpButton>
            <OpButton onClick={() => player.run('pop')}>Pop</OpButton>
            <OpButton onClick={() => player.run('peek')}>Peek</OpButton>
            <OpButton onClick={() => player.run('clear')}>Clear</OpButton>
          </form>
          {inputError && <p className="input-error">{INTEGER_HINT}</p>}
        </>
      }
      examples={{
        batch: { button: 'Push all', step: (value) => animated('push', value) },
        random: (random, count) => setUp('push', randomValues(random, count)),
      }}
      drawing={(viewBox) => (
        <StackSvg frame={view.frame} layout={view.layout} viewBox={viewBox} selectedId={selectedId}
          onSelect={(id) => setSelectedId((current) => (current === id ? null : id))} />
      )}
    />
  );
}
