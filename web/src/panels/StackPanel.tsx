import { useLayoutEffect, useRef, useState, type FormEvent } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { EngineResponse, StackOperation } from '../engine/protocol';
import type { StackLayout } from '../layout/stackLayout';
import { createAnimation, sample, totalMs, type Animation } from '../playback/animation';
import { useAnimationClock } from '../playback/useAnimationClock';
import { StackSvg } from '../render/StackSvg';

// What the value box accepts: an optional minus sign and 1 to 10 digits.
// Only the syntax is checked here; C++ checks the 32-bit range.
const INTEGER_SYNTAX = /^-?\d{1,10}$/;

export function StackPanel({ engine }: { engine: LoadedEngine }) {
  const [input, setInput] = useState('');
  const [inputError, setInputError] = useState<string | null>(null);
  const [lastResponse, setLastResponse] = useState<EngineResponse>(engine.initial);
  const [crash, setCrash] = useState<string | null>(null);
  const [animation, setAnimation] = useState<Animation>(() => createAnimation(0, engine.initial.frames, null));
  const nextAnimationId = useRef(1);

  const elapsed = useAnimationClock(animation.id, totalMs(animation));
  const { frame, layout } = sample(animation, elapsed);

  // Remember what is on screen, so the next animation starts from exactly here.
  const shownLayout = useRef<StackLayout | null>(null);
  useLayoutEffect(() => {
    shownLayout.current = layout;
  });

  // Engine calls happen only in event handlers, never during render
  // (docs/architecture.md, 2.7).
  function send(operation: StackOperation, args: Record<string, number> = {}) {
    let response: EngineResponse;
    try {
      response = engine.client.send('stack', operation, args);
    } catch (error) {
      setCrash(error instanceof Error ? error.message : String(error));
      return;
    }
    setLastResponse(response);
    // Protocol errors have no frames: the picture stays as it is.
    if (response.frames.length > 0) {
      setAnimation(createAnimation(nextAnimationId.current++, response.frames, shownLayout.current));
    }
  }

  function handlePush(event: FormEvent) {
    event.preventDefault();
    const text = input.trim();
    if (!INTEGER_SYNTAX.test(text)) {
      setInputError('Enter a whole number: an optional minus sign and 1 to 10 digits.');
      return;
    }
    setInputError(null);
    send('push', { value: Number(text) });
    setInput('');
  }

  const stepNumber = frame.index + 1;
  return (
    <section className="stack-panel" aria-labelledby="stack-heading">
      <h2 id="stack-heading">Stack</h2>

      <form className="controls" onSubmit={handlePush} noValidate>
        <label htmlFor="stack-value">Value</label>
        <input
          id="stack-value"
          type="text"
          inputMode="numeric"
          autoComplete="off"
          placeholder="e.g. 42"
          value={input}
          aria-invalid={inputError !== null}
          aria-describedby={inputError ? 'stack-value-error' : undefined}
          onChange={(event) => setInput(event.target.value)}
        />
        <button type="submit">Push</button>
        <button type="button" onClick={() => send('pop')}>Pop</button>
        <button type="button" onClick={() => send('peek')}>Peek</button>
        <button type="button" onClick={() => send('clear')}>Clear</button>
      </form>
      {inputError && (
        <p id="stack-value-error" className="input-error">{inputError}</p>
      )}

      <div className="stage">
        <StackSvg frame={frame} layout={layout} viewBox={animation.viewBox} />
      </div>

      <p className="explanation" aria-live="polite">{frame.explanation}</p>
      <p className="step-info">
        Step {stepNumber} of {animation.frames.length} · {frame.event}
      </p>

      <ResultLine response={lastResponse} />
      {crash && (
        <p className="error" role="alert">
          The C++ engine stopped: {crash} Reload the page to start again.
        </p>
      )}
    </section>
  );
}

/** One line about the latest result: what C++ returned, or its typed error. */
function ResultLine({ response }: { response: EngineResponse }) {
  const { result, operation } = response;
  if (result.status === 'error') {
    return (
      <p className="error" role="alert">
        <strong>{result.code}</strong> {result.message}
      </p>
    );
  }
  const data = result.data;
  let text: string;
  switch (operation) {
    case 'push':
      text = `push → created node #${data.nodeId}`;
      break;
    case 'pop':
      text = `pop → ${data.value} (node #${data.nodeId} freed)`;
      break;
    case 'peek':
      text = `peek → ${data.value} (node #${data.nodeId})`;
      break;
    case 'clear':
      text = `clear → freed ${data.removedCount} node${data.removedCount === 1 ? '' : 's'}`;
      break;
    default:
      return null;
  }
  return <p className="result">{text}</p>;
}
