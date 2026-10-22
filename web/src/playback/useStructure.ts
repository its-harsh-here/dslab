import { useLayoutEffect, useRef, useState } from 'react';

import type { LoadedEngine } from '../engine/loadEngine';
import type { Args, EngineResponse, Frame, Snapshot, StructureName } from '../engine/protocol';
import type { Layout, LayoutStrategy } from '../layout/layout';
import { createAnimation, sample, totalMs, type Animation } from './animation';
import { useAnimationClock } from './useAnimationClock';

export interface StructureView<S extends Snapshot> {
  /** The frame on screen now, and where its nodes are drawn. */
  readonly frame: Frame<S>;
  readonly layout: Layout;
  readonly animation: Animation<S>;
  /** The structure after the latest operation (the end of the animation). */
  readonly latest: S;
  readonly lastResponse: EngineResponse;
  readonly crash: string | null;
  /** Sends one operation to C++ and starts animating its frames. Call only from event handlers. */
  send(operation: string, args?: Args): void;
}

/** One structure tab's connection to the engine and its animation state. */
export function useStructure<S extends Snapshot>(
  engine: LoadedEngine,
  structure: StructureName,
  strategy: LayoutStrategy<S>,
): StructureView<S> {
  const initial = engine.initial[structure] as EngineResponse<S>;
  const [lastResponse, setLastResponse] = useState<EngineResponse>(initial);
  const [crash, setCrash] = useState<string | null>(null);
  const [animation, setAnimation] = useState(() => createAnimation(0, initial.frames, null, strategy));
  const nextAnimationId = useRef(1);

  const elapsed = useAnimationClock(animation.id, totalMs(animation));
  const { frame, layout } = sample(animation, elapsed);

  // Remember what is on screen, so the next animation starts from exactly here.
  const shownLayout = useRef<Layout | null>(null);
  useLayoutEffect(() => {
    shownLayout.current = layout;
  });

  function send(operation: string, args: Args = {}) {
    let response: EngineResponse<S>;
    try {
      response = engine.client.send(structure, operation, args) as EngineResponse<S>;
    } catch (error) {
      setCrash(error instanceof Error ? error.message : String(error));
      return;
    }
    setLastResponse(response);
    // Protocol errors have no frames: the picture stays as it is.
    if (response.frames.length > 0) {
      setAnimation(createAnimation(nextAnimationId.current++, response.frames, shownLayout.current, strategy));
    }
  }

  const latest = animation.frames[animation.frames.length - 1].snapshot;
  return { frame, layout, animation, latest, lastResponse, crash, send };
}
