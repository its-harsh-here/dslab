// Undo/redo and export/import from the page.
import { readFileSync } from 'node:fs';

import type { Page } from '@playwright/test';

import { drawnNodeIds, expect, expectReady, expectStepMatchesFrame, finish, open, replay, test, type Request } from './support';

async function exportText(page: Page): Promise<string> {
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByRole('button', { name: 'Export', exact: true }).click()]);
  expect(download.suggestedFilename()).toBe('dslab-avl.json');
  return readFileSync((await download.path())!, 'utf8');
}

async function importText(page: Page, text: string) {
  await page.locator('input[type=file]').setInputFiles({ name: 'dslab-avl.json', mimeType: 'application/json', buffer: Buffer.from(text) });
}

const inserts = (values: number[]): Request[] => values.map((value) => ({ structure: 'avl', operation: 'insert', args: { value } }));

test('undo and redo (buttons and Ctrl+Z / Ctrl+Y) show exactly the earlier states', async ({ page }) => {
  await open(page, 'avl');
  await page.locator('#batch-values').fill('30, 20, 10, 40, 50');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);

  const requests: Request[] = [...inserts([30, 20, 10, 40, 50])];
  const steps: [string, () => Promise<void>][] = [
    ['undo', () => page.getByRole('button', { name: 'Undo', exact: true }).click()],
    ['undo', () => page.keyboard.press('Control+z')],
    ['undo', () => page.getByRole('button', { name: 'Undo', exact: true }).click()],
    ['redo', () => page.getByRole('button', { name: 'Redo', exact: true }).click()],
    ['redo', () => page.keyboard.press('Control+y')],
  ];
  for (const [operation, act] of steps) {
    await act();
    await finish(page);
    requests.push({ structure: 'avl', operation });
    const last = (await replay(page, requests)).at(-1)!;
    expect(last.result.status).toBe('ok');
    await expectStepMatchesFrame(page, last.frames.at(-1)!, last.frames.length - 1, last.frames.length, 'tree');
  }
  // One redo is left; a new operation clears it.
  await expect(page.getByRole('button', { name: 'Redo', exact: true })).toBeEnabled();
  await page.locator('#avltree-value').fill('99');
  await page.getByRole('button', { name: 'Insert', exact: true }).click();
  await finish(page);
  await expect(page.getByRole('button', { name: 'Redo', exact: true })).toBeDisabled();
});

test('export, reset and import restore the same tree; an invalid import changes nothing', async ({ page }) => {
  await open(page, 'avl');
  await page.locator('#batch-values').fill('50, 30, 70, 20, 40, 60, 80, 10');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);
  const saved = await exportText(page);
  const treeBefore = await drawnNodeIds(page);

  await page.getByRole('button', { name: 'Reset', exact: true }).click();
  await expectReady(page);
  expect(await drawnNodeIds(page)).toEqual([]);

  await importText(page, saved);
  await finish(page);
  expect(await drawnNodeIds(page)).toEqual(treeBefore);
  expect(await exportText(page)).toBe(saved); // byte for byte, ID counter included

  // A broken file: one stored balance changed.
  const broken = JSON.parse(saved);
  broken.snapshot.nodes[0].balance += 2;
  await importText(page, JSON.stringify(broken, null, 2));
  await expectReady(page);
  await expect(page.locator('.result.error')).toContainText('IMPORT_INVALID');
  await expect(page.locator('.result.error')).toContainText('Node 1 stores balance');
  await expect(page.locator('.history-list li').last()).toContainText('IMPORT_INVALID');
  expect(await drawnNodeIds(page)).toEqual(treeBefore);
  expect(await exportText(page)).toBe(saved);

  // Not JSON at all.
  await importText(page, 'this is not a DSLab file');
  await expectReady(page);
  await expect(page.locator('.result.error')).toContainText('The file is not valid JSON.');
  expect(await exportText(page)).toBe(saved);
});
