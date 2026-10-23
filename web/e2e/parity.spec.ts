// Native versus Wasm: the same command fixtures (web/e2e/fixtures/parity-commands.jsonl,
// written by scripts/make-parity-commands.py) through the native command runner
// (build/native/dslab_cli) and through the Wasm engine of the production build.
// Every response is normalized (parsed and written out again) and compared.
// Also: invalid requests never change state, and engines can be created and
// destroyed many times.
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';

import type { Page } from '@playwright/test';

import { expect, open, test } from './support';

const commands = readFileSync('web/e2e/fixtures/parity-commands.jsonl', 'utf8').trimEnd().split('\n');

function runNative(lines: readonly string[]): string[] {
  const output = execFileSync('build/native/dslab_cli', { input: lines.join('\n') + '\n', maxBuffer: 1024 * 1024 * 1024 });
  return output.toString('utf8').trimEnd().split('\n');
}

const normalize = (text: string) => JSON.stringify(JSON.parse(text));
const sha256 = (text: string) => createHash('sha256').update(text).digest('hex');

/** Runs the commands on one fresh Wasm engine in the page; returns the SHA-256 of each normalized response. */
function runWasm(page: Page, lines: readonly string[]): Promise<string[]> {
  return page.evaluate(async (list) => {
    const hook = (window as unknown as { __dslab: { createEngine(): { handle(text: string): string; delete(): void } } }).__dslab;
    const engine = hook.createEngine();
    const hashes: string[] = [];
    for (const line of list) {
      const normalized = JSON.stringify(JSON.parse(engine.handle(line)));
      const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(normalized));
      hashes.push([...new Uint8Array(digest)].map((byte) => byte.toString(16).padStart(2, '0')).join(''));
    }
    engine.delete();
    return hashes;
  }, lines);
}

test('the native and Wasm builds answer every command identically', async ({ page }) => {
  test.setTimeout(300_000);
  const native = runNative(commands);
  expect(native).toHaveLength(commands.length);
  await open(page, 'stack');
  const wasm = await runWasm(page, commands);
  const nativeHashes = native.map((text) => sha256(normalize(text)));
  const first = nativeHashes.findIndex((hash, index) => hash !== wasm[index]);
  expect(first, first < 0 ? '' : `first difference at command ${first + 1}: ${commands[first].slice(0, 160)}`).toBe(-1);
  expect(wasm).toHaveLength(commands.length);
});

test('invalid requests never change state (checked on the native answers, equal to Wasm above)', async () => {
  const native = runNative(commands).map((text) => JSON.parse(text));
  let checkedWrapped = 0;
  let checkedErrors = 0;
  const lastSnapshot = new Map<string, unknown>();
  for (let i = 0; i < commands.length; i++) {
    const response = native[i];
    const structure: string | null = response.structure;
    // Every error leaves the snapshot exactly as the previous response of that structure showed it.
    if (response.result.status === 'error' && structure !== null && lastSnapshot.has(structure)) {
      expect(response.snapshot, `command ${i + 1} changed the ${structure}`).toEqual(lastSnapshot.get(structure));
      checkedErrors++;
    }
    if (structure !== null && response.snapshot !== null) lastSnapshot.set(structure, response.snapshot);
    // The deliberately invalid requests sit between two exports: the documents (ID counter included) are equal.
    const isExport = (k: number) => native[k]?.operation === 'export' && native[k].structure === structure;
    if (i >= 1 && isExport(i - 1) && isExport(i + 1) && response.result.status === 'error') {
      expect(native[i + 1].result.data.text, `command ${i + 1}`).toBe(native[i - 1].result.data.text);
      checkedWrapped++;
    }
  }
  expect(checkedWrapped).toBeGreaterThanOrEqual(30);
  expect(checkedErrors).toBeGreaterThanOrEqual(60);
});

test('Wasm: 300 engines created, used and destroyed; the next one still answers exactly like native', async ({ page }) => {
  test.setTimeout(300_000);
  await open(page, 'stack');
  const sample = commands.slice(0, 40);
  const count = await page.evaluate((list) => {
    const hook = (window as unknown as { __dslab: { createEngine(): { handle(text: string): string; delete(): void } } }).__dslab;
    let created = 0;
    for (let round = 0; round < 300; round++) {
      const engine = hook.createEngine();
      for (const line of list) engine.handle(line);
      // Reset every structure, then destroy the engine.
      for (const structure of ['stack', 'queue', 'list', 'binaryTree', 'bst', 'avl', 'redBlack']) {
        engine.handle(JSON.stringify({ schemaVersion: 1, requestId: 1, structure, operation: 'clear', args: {} }));
      }
      engine.delete();
      created++;
    }
    return created;
  }, sample);
  expect(count).toBe(300);
  const head = commands.slice(0, 200);
  expect(await runWasm(page, head)).toEqual(runNative(head).map((text) => sha256(normalize(text))));
});
