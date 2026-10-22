import { useState, type FormEvent } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { StackSnapshot } from '../engine/protocol';
import { stackLayout } from '../layout/linear';
import { useStructure } from '../playback/useStructure';
import { StackSvg } from '../render/StackSvg';
import { INTEGER_HINT, IntegerField, readInteger, StepReport } from './common';

export function StackPanel({ engine }: { engine: LoadedEngine }) {
  const view = useStructure<StackSnapshot>(engine, 'stack', stackLayout);
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState(false);

  // Engine calls happen only in event handlers, never during render
  // (docs/architecture.md, 2.7).
  function handlePush(event: FormEvent) {
    event.preventDefault();
    const value = readInteger(input);
    setInputError(value === null);
    if (value === null) return;
    view.send('push', { value });
    setInput('');
  }

  return (
    <section className="structure-panel" aria-labelledby="stack-heading">
      <h2 id="stack-heading">Stack</h2>
      <form className="controls" onSubmit={handlePush} noValidate>
        <IntegerField id="stack-value" label="Value" value={input} onChange={setInput} invalid={inputError} />
        <button type="submit">Push</button>
        <button type="button" onClick={() => view.send('pop')}>Pop</button>
        <button type="button" onClick={() => view.send('peek')}>Peek</button>
        <button type="button" onClick={() => view.send('clear')}>Clear</button>
      </form>
      {inputError && <p className="input-error">{INTEGER_HINT}</p>}

      <div className="stage stage-bottom">
        <StackSvg frame={view.frame} layout={view.layout} viewBox={view.animation.viewBox} />
      </div>
      <StepReport view={view} />
    </section>
  );
}
