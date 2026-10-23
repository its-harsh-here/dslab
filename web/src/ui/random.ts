// Seeded random input for the "random example" button. It only produces input
// values (and, for the binary tree, which free child slot to use); every
// structure rule is still applied by C++.

/** mulberry32: a small, well-known 32-bit generator. The same seed gives the same sequence. */
export function seededRandom(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** A whole number from `low` to `high`, both included. */
export function randomInt(random: () => number, low: number, high: number): number {
  return low + Math.floor(random() * (high - low + 1));
}

/** `count` different whole numbers from 1 to 99, in random order (for the search trees). */
export function distinctValues(random: () => number, count: number): number[] {
  const values: number[] = [];
  while (values.length < Math.min(count, 99)) {
    const value = randomInt(random, 1, 99);
    if (!values.includes(value)) values.push(value);
  }
  return values;
}
