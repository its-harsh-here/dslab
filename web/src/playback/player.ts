// One structure's player: the authoritative engine state, the operation shown
// on screen, playback timing, waiting batch steps and the operation history.
//
// It lives outside React (components read it through useSyncExternalStore), so:
//  - there is exactly one requestAnimationFrame loop per structure, started and
//    stopped only here, never by a React effect that StrictMode could run twice;
//  - a structure keeps its state while another one is on screen.
//
// Two states are kept apart on purpose:
//  - `latest` is the structure as C++ holds it now (the newest response);
//  - `animation` + `index` is the historical frame being shown.
// Operations that need the structure (the next list index, a random free slot)
// read `latest`; drawings and panels read the shown frame.
import type { EngineClient } from '../engine/client';
import type { Args, EngineResponse, Result, Snapshot, StructureName } from '../engine/protocol';
import type { LayoutStrategy } from '../layout/layout';
import { createAnimation, finalLayout, type Animation } from './animation';

export const SPEEDS = [0.5, 1, 2, 4] as const;
export type Speed = (typeof SPEEDS)[number];

/** How long one frame stays on screen at 1×, and how much of that its nodes spend moving. */
const FRAME_MS = 600;
const MOVE_MS = 360;
const HISTORY_LIMIT = 100;

/** One request of a batch. `args` may be computed from `latest` when the request is sent. */
export interface Step<S extends Snapshot> {
  readonly operation: string;
  readonly args?: Args | ((latest: S) => Args);
  /** Play its frames, or (set-up steps) show only its result. */
  readonly animate: boolean;
}

export interface HistoryEntry {
  readonly id: number;
  readonly operation: string;
  readonly args: Args;
  readonly result: Result;
  readonly frames: number;
}

export interface PlayerState<S extends Snapshot> {
  /** The structure as C++ holds it now. */
  readonly latest: S;
  /** The operation on screen: its request (null for the state at page load), response and frames laid out. */
  readonly shown: HistoryEntry | null;
  readonly response: EngineResponse<S>;
  readonly animation: Animation<S>;
  /** The frame on screen, and how far (0 to 1) its nodes have moved into place. */
  readonly index: number;
  readonly t: number;
  readonly playing: boolean;
  readonly speed: Speed;
  /** Batch steps not sent yet. */
  readonly queued: number;
  /** How many operations C++ can undo and redo for this structure. */
  readonly undoCount: number;
  readonly redoCount: number;
  /** The newest request and its result (a protocol error has no frames to show). */
  readonly lastOperation: string | null;
  readonly lastResult: Result | null;
  readonly history: readonly HistoryEntry[];
  readonly crash: string | null;
}

/** True while an operation plays, a batch waits, or an earlier frame than the result is shown. */
export function isBusy(state: PlayerState<Snapshot>): boolean {
  return state.playing || state.queued > 0 || state.t < 1 || state.index < state.animation.frames.length - 1;
}

function reducedMotion(): boolean {
  return window.matchMedia('(prefers-reduced-motion: reduce)').matches;
}

export class Player<S extends Snapshot> {
  private state: PlayerState<S>;
  private readonly listeners = new Set<() => void>();
  private queue: Step<S>[] = [];
  /** Milliseconds (at 1×) since the frame on screen appeared. */
  private elapsed = MOVE_MS;
  /** "Step forward": run the loop only until this frame's nodes are in place. */
  private stopAfterMove = false;
  /** The waiting requestAnimationFrame callback, or 0 when the loop is stopped. */
  private handle = 0;
  private lastTime = 0;
  private nextHistoryId = 1;
  /** The newest response, for run(). */
  private newest: EngineResponse<S> | null = null;

  constructor(
    private readonly client: EngineClient,
    readonly structure: StructureName,
    private readonly strategy: LayoutStrategy<S>,
    initial: EngineResponse<S>,
  ) {
    const animation = createAnimation(initial.frames, null, strategy);
    this.state = {
      latest: initial.snapshot as S,
      shown: null,
      response: initial,
      animation,
      index: animation.frames.length - 1,
      t: 1,
      playing: false,
      speed: 1,
      queued: 0,
      undoCount: initial.history?.undo ?? 0,
      redoCount: initial.history?.redo ?? 0,
      lastOperation: null,
      lastResult: null,
      history: [],
      crash: null,
    };
  }

  readonly subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  };

  readonly getState = (): PlayerState<S> => this.state;

  private update(changes: Partial<PlayerState<S>>) {
    this.state = { ...this.state, ...changes };
    for (const listener of this.listeners) listener();
  }

  private lastIndex(): number {
    return this.state.animation.frames.length - 1;
  }

  // --- Operations -----------------------------------------------------------

  /**
   * Runs one operation and plays it (or, with animate false, shows its result).
   * Returns C++'s response, or null when refused because the player is busy.
   */
  run(operation: string, args: Args = {}, animate = true): EngineResponse<S> | null {
    return this.runSteps([{ operation, args, animate }]) ? this.newest : null;
  }

  /**
   * Runs steps one at a time, in order. A set-up step is sent and shows only its
   * result; an animated step is sent and played, and the steps after it are sent
   * only when its playback has ended. Refused (returns false) while busy.
   */
  runSteps(steps: readonly Step<S>[]): boolean {
    if (isBusy(this.state) || steps.length === 0 || this.state.crash) return false;
    this.queue = [...steps];
    this.sendQueued();
    return true;
  }

  /** Shows the result of the shown operation, finishing a waiting batch (unanimated, in order). */
  jumpToResult() {
    this.stop();
    while (this.queue.length > 0) {
      const step = this.queue.shift() as Step<S>;
      if (this.send(step, false) === 'crashed') this.queue = [];
    }
    this.settleAt(this.lastIndex());
    this.update({ playing: false, queued: 0 });
  }

  /** Stops playback and drops waiting batch steps (used when another structure is chosen). */
  cancel() {
    this.stop();
    this.queue = [];
    this.settleAt(this.lastIndex());
    this.update({ playing: false, queued: 0 });
  }

  /** Cancels everything, clears the structure in C++ (unanimated) and empties the history. */
  reset() {
    this.cancel();
    if (this.state.crash) return;
    this.send({ operation: 'clear', animate: false }, false);
    this.update({ history: [], shown: null, lastOperation: null, lastResult: null });
  }

  private sendQueued() {
    while (this.queue.length > 0) {
      const step = this.queue.shift() as Step<S>;
      const outcome = this.send(step, step.animate);
      if (outcome === 'crashed') {
        this.queue = [];
        break;
      }
      if (outcome === 'playing') break; // the rest wait until this one has played
    }
    this.update({ queued: this.queue.length });
  }

  /**
   * Sends one request to C++ and shows its frames: 'playing' when they now play,
   * 'shown' when only the result is shown (or nothing ran: a protocol error),
   * 'crashed' when the engine stopped.
   */
  private send(step: Step<S>, animate: boolean): 'playing' | 'shown' | 'crashed' {
    const args = typeof step.args === 'function' ? step.args(this.state.latest) : (step.args ?? {});
    let response: EngineResponse<S>;
    try {
      response = this.client.send(this.structure, step.operation, args) as EngineResponse<S>;
    } catch (error) {
      this.stop();
      this.update({ crash: error instanceof Error ? error.message : String(error), playing: false });
      return 'crashed';
    }
    this.newest = response;
    const entry: HistoryEntry = {
      id: this.nextHistoryId++,
      operation: step.operation,
      // An imported file's text is not kept in the log.
      args: Object.fromEntries(Object.entries(args).filter(([key]) => key !== 'text')),
      result: response.result,
      frames: response.frames.length,
    };
    const history = [...this.state.history, entry].slice(-HISTORY_LIMIT);
    const lastResult = {
      lastOperation: step.operation,
      lastResult: response.result,
      history,
      undoCount: response.history?.undo ?? this.state.undoCount,
      redoCount: response.history?.redo ?? this.state.redoCount,
    };

    // A protocol error ran nothing: no frames, and the picture stays as it is.
    if (response.frames.length === 0) {
      this.update(lastResult);
      return 'shown';
    }
    // Requests are only sent when the shown operation is at its result, so its
    // final layout is exactly what is on screen.
    const animation = createAnimation(response.frames, finalLayout(this.state.animation), this.strategy);
    const play = animate && !reducedMotion() ? 0 : 1;
    this.elapsed = animate ? 0 : MOVE_MS;
    this.update({
      ...lastResult,
      latest: (response.snapshot ?? this.state.latest) as S,
      shown: entry,
      response,
      animation,
      index: animate ? 0 : animation.frames.length - 1,
      t: animate ? play : 1,
      playing: animate,
    });
    if (!animate) return 'shown';
    this.start();
    return 'playing';
  }

  // --- Playback -------------------------------------------------------------

  play() {
    if (this.state.playing) return;
    if (this.state.index === this.lastIndex() && this.state.t >= 1) {
      this.replay();
      return;
    }
    this.stopAfterMove = false;
    this.update({ playing: true });
    this.start();
  }

  /** Pauses on a still frame: a move in progress is completed at once. */
  pause() {
    this.stop();
    this.elapsed = Math.max(this.elapsed, MOVE_MS);
    this.update({ playing: false, t: 1 });
  }

  togglePlay() {
    if (this.state.playing) this.pause();
    else this.play();
  }

  /** The next frame, with its nodes moving into place, then pause. */
  stepForward() {
    this.stop();
    const next = Math.min(this.state.index + 1, this.lastIndex());
    if (next === this.state.index || reducedMotion()) {
      this.settleAt(next);
      this.update({ playing: false });
      return;
    }
    this.elapsed = 0;
    this.update({ index: next, t: 0, playing: false });
    this.stopAfterMove = true;
    this.start();
  }

  stepBack() {
    this.stop();
    this.settleAt(Math.max(this.state.index - 1, 0));
    this.update({ playing: false });
  }

  /** Shows frame `index` (scrubber, first frame), paused. */
  seek(index: number) {
    this.stop();
    this.settleAt(Math.max(0, Math.min(index, this.lastIndex())));
    this.update({ playing: false });
  }

  replay() {
    this.stop();
    this.elapsed = 0;
    this.update({ index: 0, t: reducedMotion() ? 1 : 0, playing: true });
    this.start();
  }

  setSpeed(speed: Speed) {
    this.update({ speed });
  }

  /** Frame `index` with its nodes already in place. */
  private settleAt(index: number) {
    this.elapsed = MOVE_MS;
    this.update({ index, t: 1 });
  }

  // --- The animation loop ---------------------------------------------------

  private start() {
    if (this.handle !== 0) return; // one loop at most
    this.lastTime = performance.now();
    this.handle = requestAnimationFrame(this.tick);
  }

  private stop() {
    if (this.handle !== 0) cancelAnimationFrame(this.handle);
    this.handle = 0;
    this.stopAfterMove = false;
  }

  private readonly tick = (now: number) => {
    this.handle = 0;
    // A frame's timestamp can be slightly earlier than the time we started at.
    const dt = Math.max(0, now - this.lastTime);
    this.lastTime = now;
    this.elapsed += dt * this.state.speed;
    const moving = !reducedMotion();
    const progress = () => (moving ? Math.min(1, this.elapsed / MOVE_MS) : 1);

    if (this.stopAfterMove) {
      const t = progress();
      this.update({ t });
      if (t < 1) this.handle = requestAnimationFrame(this.tick);
      else this.stopAfterMove = false;
      return;
    }
    if (!this.state.playing) return;

    let index = this.state.index;
    const last = this.lastIndex();
    while (this.elapsed >= FRAME_MS && index < last) {
      index += 1;
      this.elapsed -= FRAME_MS;
    }
    if (index === last && this.elapsed >= FRAME_MS) {
      // The end of this operation: stop on its result, then send the next batch step.
      this.settleAt(last);
      this.update({ playing: false });
      if (this.queue.length > 0) this.sendQueued();
      return;
    }
    this.update({ index, t: progress() });
    this.handle = requestAnimationFrame(this.tick);
  };
}
