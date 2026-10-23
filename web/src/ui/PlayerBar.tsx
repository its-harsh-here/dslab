// Below the drawing: playback controls and the timeline of the operation on screen.
import type { Snapshot } from '../engine/protocol';
import { SPEEDS, type Player, type Speed } from '../playback/player';
import type { PlayerView } from '../playback/usePlayer';
import { eventLabel } from './describe';

export function PlayerBar<S extends Snapshot>({ player, view }: { player: Player<S>; view: PlayerView<S> }) {
  const { state, frame, busy } = view;
  const total = state.animation.frames.length;
  const atStart = state.index === 0;
  const atEnd = state.index === total - 1;

  return (
    <div className="player" role="group" aria-label="Playback">
      <div className="player-buttons">
        <button type="button" onClick={() => player.replay()} aria-keyshortcuts="R" disabled={total < 2}>
          Replay
        </button>
        <button type="button" onClick={() => player.stepBack()} aria-keyshortcuts="ArrowLeft" disabled={atStart}>
          Step back
        </button>
        <button type="button" className="play" onClick={() => player.togglePlay()} aria-keyshortcuts="Space K"
          disabled={total < 2}>
          {state.playing ? 'Pause' : 'Play'}
        </button>
        <button type="button" onClick={() => player.stepForward()} aria-keyshortcuts="ArrowRight" disabled={atEnd}>
          Step forward
        </button>
        <button type="button" onClick={() => player.jumpToResult()} aria-keyshortcuts="End" disabled={!busy}>
          Jump to result
        </button>
        <label className="speed">
          Speed
          <select value={state.speed} onChange={(event) => player.setSpeed(Number(event.target.value) as Speed)}
            aria-keyshortcuts="[ ]">
            {SPEEDS.map((speed) => (
              <option key={speed} value={speed}>{speed}×</option>
            ))}
          </select>
        </label>
      </div>
      <div className="timeline">
        <input type="range" min={0} max={Math.max(total - 1, 0)} step={1} value={state.index}
          onChange={(event) => player.seek(Number(event.target.value))}
          aria-label="Step of the operation" aria-valuetext={`Step ${state.index + 1} of ${total}: ${eventLabel(frame.event)}`} />
        <span className="step-count" aria-hidden="true">
          Step {state.index + 1} of {total}
        </span>
      </div>
    </div>
  );
}
