// Below the player: every request sent for this structure, in order.
import { useLayoutEffect, useRef } from 'react';

import type { HistoryEntry } from '../playback/player';
import { describeRequest, describeResult } from './describe';

export function HistoryLog({ history }: { history: readonly HistoryEntry[] }) {
  const list = useRef<HTMLOListElement>(null);
  // Keep the newest entry in view.
  useLayoutEffect(() => {
    if (list.current) list.current.scrollTop = list.current.scrollHeight;
  }, [history.length]);

  return (
    <section className="history" aria-labelledby="history-heading">
      <h3 id="history-heading">Operation history</h3>
      {history.length === 0 ? (
        <p className="meta">No operations yet on this structure.</p>
      ) : (
        <ol ref={list} className="history-list">
          {history.map((entry) => (
            <li key={entry.id} className={entry.result.status === 'error' ? 'failed' : undefined}>
              <span className="history-request">{describeRequest(entry.operation, entry.args)}</span>
              <span className="history-result">
                {entry.result.status === 'ok' ? 'ok: ' : ''}
                {describeResult(entry.operation, entry.result)}
              </span>
              <span className="history-frames">{entry.frames} {entry.frames === 1 ? 'step' : 'steps'}</span>
            </li>
          ))}
        </ol>
      )}
    </section>
  );
}
