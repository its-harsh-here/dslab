// Building blocks for the examples bar: input values only. Every request
// still goes to C++ one at a time, which applies all the rules.
import type { Snapshot } from '../engine/protocol';
import type { Step } from '../playback/player';
import { randomInt } from '../ui/random';

/** Clear, then send `operation` once per value, showing only results (a set-up). */
export function setUp<S extends Snapshot>(operation: string, values: readonly number[]): Step<S>[] {
  return [
    { operation: 'clear', animate: false },
    ...values.map((value): Step<S> => ({ operation, args: { value }, animate: false })),
  ];
}

/** `count` values from 1 to 99 (repeats allowed). */
export function randomValues(random: () => number, count: number): number[] {
  return Array.from({ length: count }, () => randomInt(random, 1, 99));
}

/** One animated request with a value. */
export function animated<S extends Snapshot>(operation: string, value: number): Step<S> {
  return { operation, args: { value }, animate: true };
}
