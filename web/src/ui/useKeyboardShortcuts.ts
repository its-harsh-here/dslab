// The keyboard shortcuts, as one listener on the window for the structure on
// screen. It is replaced (never added twice) when the structure changes.
import { useEffect, type RefObject } from 'react';

import type { Snapshot } from '../engine/protocol';
import { SPEEDS, type Player } from '../playback/player';
import type { StageCommands } from './Stage';

const PAN_STEP = 60; // pixels

export const SHORTCUTS: readonly [string, string][] = [
  ['Space or K', 'play / pause'],
  ['← / →', 'step back / forward'],
  ['Home / End', 'first step / jump to result'],
  ['R', 'replay'],
  ['[ / ]', 'slower / faster'],
  ['+ / − / 0', 'zoom in / out / fit'],
  ['Shift + arrows', 'pan'],
  ['Ctrl+Z / Ctrl+Y', 'undo / redo the last operation'],
  ['Esc', 'leave a text box'],
];

export function useKeyboardShortcuts(player: Player<Snapshot>, stage: RefObject<StageCommands | null>) {
  useEffect(() => {
    function onKeyDown(event: KeyboardEvent) {
      const target = event.target instanceof HTMLElement ? event.target : null;
      const typing = target?.closest('input, textarea, select, [contenteditable="true"]') != null;
      // Undo and redo whole operations (in a text box, Ctrl+Z keeps undoing typing).
      if ((event.ctrlKey || event.metaKey) && !event.altKey && !typing) {
        const key = event.key.toLowerCase();
        if (key === 'z' && !event.shiftKey) player.run('undo');
        else if (key === 'y' || (key === 'z' && event.shiftKey)) player.run('redo');
        else return;
        event.preventDefault();
        return;
      }
      if (event.defaultPrevented || event.altKey || event.ctrlKey || event.metaKey) return;
      // Typing in a box, or using a select or slider, keeps its normal keys; Esc leaves the box.
      if (target?.closest('input, textarea, select, [contenteditable="true"]')) {
        if (event.key === 'Escape') target.blur();
        return;
      }
      // A focused button already answers Space and Enter.
      const onButton = target !== null && target.closest('button, summary, a, [role="button"]') !== null;
      const speedIndex = SPEEDS.indexOf(player.getState().speed);
      switch (event.key) {
        case ' ':
          if (onButton) return;
          player.togglePlay();
          break;
        case 'k':
        case 'K':
          player.togglePlay();
          break;
        case 'ArrowLeft':
          if (event.shiftKey) stage.current?.pan(PAN_STEP, 0);
          else player.stepBack();
          break;
        case 'ArrowRight':
          if (event.shiftKey) stage.current?.pan(-PAN_STEP, 0);
          else player.stepForward();
          break;
        case 'ArrowUp':
        case 'ArrowDown':
          if (!event.shiftKey) return; // plain arrows keep scrolling the page
          stage.current?.pan(0, event.key === 'ArrowUp' ? PAN_STEP : -PAN_STEP);
          break;
        case 'Home':
          player.seek(0);
          break;
        case 'End':
          player.jumpToResult();
          break;
        case 'r':
        case 'R':
          player.replay();
          break;
        case '[':
          player.setSpeed(SPEEDS[Math.max(0, speedIndex - 1)]);
          break;
        case ']':
          player.setSpeed(SPEEDS[Math.min(SPEEDS.length - 1, speedIndex + 1)]);
          break;
        case '+':
        case '=':
          stage.current?.zoomIn();
          break;
        case '-':
          stage.current?.zoomOut();
          break;
        case '0':
          stage.current?.fit();
          break;
        default:
          return;
      }
      event.preventDefault();
    }
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [player, stage]);
}
