import { useSyncExternalStore } from 'react';

import type { Frame, Snapshot } from '../engine/protocol';
import type { Layout } from '../layout/layout';
import { sample } from './animation';
import { isBusy, type Player, type PlayerState } from './player';

/** What a structure's page draws: the player's state and the frame on screen. */
export interface PlayerView<S extends Snapshot> {
  readonly state: PlayerState<S>;
  readonly frame: Frame<S>;
  readonly layout: Layout;
  /** New operations wait while this is true (playing, a batch waiting, or not at the result). */
  readonly busy: boolean;
}

export function usePlayer<S extends Snapshot>(player: Player<S>): PlayerView<S> {
  const state = useSyncExternalStore(player.subscribe, player.getState);
  const { frame, layout } = sample(state.animation, state.index, state.t);
  return { state, frame, layout, busy: isBusy(state) };
}
