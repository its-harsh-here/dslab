// Playback correctness: pause and resume, rapid clicking, timeline scrubbing,
// jump to result, reset and structure switching during playback.
import {
  drawnNodeIds,
  expect,
  expectReady,
  expectStepMatchesFrame,
  finish,
  open,
  replay,
  seek,
  setSpeed,
  statusLine,
  stepOf,
  test,
  type Request,
} from './support';

const historyEntries = (page: import('@playwright/test').Page) =>
  page.locator('.history-list .history-request').allTextContents();

test('pause holds the step; play resumes from it to the end', async ({ page }) => {
  await open(page, 'redBlack');
  await page.locator('#batch-values').fill('10, 20, 40, 30');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);

  await page.locator('#redblacktree-value').fill('10');
  await page.getByRole('button', { name: 'Delete', exact: true }).click(); // 18 steps at 600 ms
  await expect.poll(async () => (await stepOf(page))[0]).toBeGreaterThanOrEqual(3);
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  const [paused, total] = await stepOf(page);
  await page.waitForTimeout(1500);
  expect((await stepOf(page))[0], 'no step advances while paused').toBe(paused);
  await expect(statusLine(page)).toContainText(`Showing delete 10: step ${paused} of ${total}`);
  await expect(page.getByRole('button', { name: 'Insert', exact: true })).toBeDisabled();

  await page.getByRole('button', { name: 'Play', exact: true }).click();
  await expect.poll(async () => (await stepOf(page))[0]).toBeGreaterThan(paused);
  await expectReady(page);
  expect(await stepOf(page)).toEqual([total, total]);
  await expect(page.getByRole('button', { name: 'Insert', exact: true })).toBeEnabled();
});

test('rapid clicking starts exactly one operation at a time', async ({ page }) => {
  await open(page, 'bst');
  await page.locator('#binarysearchtree-value').fill('42');
  // Ten clicks in one burst, before React can even re-render the disabled button.
  await page.getByRole('button', { name: 'Insert', exact: true }).evaluate((button) => {
    for (let i = 0; i < 10; i++) (button as HTMLButtonElement).click();
  });
  await finish(page);
  expect(await historyEntries(page)).toEqual(['insert 42']);
  expect(await drawnNodeIds(page)).toEqual([1]);

  // The same for Undo: one undo, not ten.
  await page.getByRole('button', { name: 'Undo', exact: true }).evaluate((button) => {
    for (let i = 0; i < 10; i++) (button as HTMLButtonElement).click();
  });
  await finish(page);
  expect(await historyEntries(page)).toEqual(['insert 42', 'undo']);
  expect(await drawnNodeIds(page)).toEqual([]);
  await expect(page.getByRole('button', { name: 'Redo', exact: true })).toBeEnabled();
});

test('rapid stepping lands on the first and last steps, and the screen matches each frame', async ({ page }) => {
  await open(page, 'avl');
  await setSpeed(page, '0.5');
  await page.locator('#batch-values').fill('30, 10');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);
  await page.locator('#avltree-value').fill('20');
  await page.getByRole('button', { name: 'Insert', exact: true }).click();
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  const responses = await replay(page, [30, 10, 20].map((value): Request => ({ structure: 'avl', operation: 'insert', args: { value } })));
  const frames = responses.at(-1)!.frames;

  await page.getByRole('button', { name: 'Step forward', exact: true }).evaluate((button) => {
    for (let i = 0; i < 50; i++) (button as HTMLButtonElement).click();
  });
  await expect.poll(async () => stepOf(page)).toEqual([frames.length, frames.length]);
  await page.getByRole('button', { name: 'Step back', exact: true }).evaluate((button) => {
    for (let i = 0; i < 50; i++) (button as HTMLButtonElement).click();
  });
  await expect.poll(async () => stepOf(page)).toEqual([1, frames.length]);
  await expectStepMatchesFrame(page, frames[0], 0, frames.length, 'tree');

  // Keyboard: the right arrow steps forward (focus outside text boxes).
  await page.locator('body').click({ position: { x: 5, y: 5 } });
  for (let i = 0; i < 4; i++) await page.keyboard.press('ArrowRight');
  await expect.poll(async () => stepOf(page)).toEqual([5, frames.length]);
});

test('timeline scrubbing shows each requested frame; jump to result ends the operation', async ({ page }) => {
  await open(page, 'bst');
  await page.locator('#batch-values').fill('50, 30, 70, 20, 40, 60, 80');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);
  await page.getByRole('button', { name: 'Level-order', exact: true }).click();
  const requests: Request[] = [50, 30, 70, 20, 40, 60, 80].map((value) => ({ structure: 'bst', operation: 'insert', args: { value } }));
  const responses = await replay(page, [...requests, { structure: 'bst', operation: 'levelorder' }]);
  const frames = responses.at(-1)!.frames;

  // Back and forth, out of order. Operations wait on every intermediate step;
  // the last step is the result, so there they are allowed again.
  const n = frames.length;
  for (const index of [n - 1, 0, 5, 2, n - 2, 1, 7, 3]) {
    await seek(page, index);
    await expectStepMatchesFrame(page, frames[index], index, frames.length, 'tree');
    const insert = page.getByRole('button', { name: 'Insert', exact: true });
    if (index === frames.length - 1) await expect(insert).toBeEnabled();
    else await expect(insert).toBeDisabled();
  }
  await page.locator('.status-line button', { hasText: 'Jump to result' }).click();
  await expectReady(page);
  await expectStepMatchesFrame(page, frames.at(-1)!, frames.length - 1, frames.length, 'tree');
  await expect(page.getByRole('button', { name: 'Insert', exact: true })).toBeEnabled();
});

test('switching structure during a batch cancels it cleanly', async ({ page }) => {
  await open(page, 'redBlack');
  await page.locator('#batch-values').fill('1, 2, 3, 4, 5, 6, 7, 8');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await expect(statusLine(page)).toContainText('more queued');
  // Through every other structure and back, quickly.
  for (const name of ['Stack', 'Queue', 'Linked list', 'Binary tree', 'Binary search tree', 'AVL tree', 'Red-black tree']) {
    await page.locator('.structure-nav button', { hasText: name }).first().click();
    await expectReady(page);
  }
  // Only the first insert was sent; the queued ones were dropped.
  expect(await historyEntries(page)).toEqual(['insert 1']);
  expect(await drawnNodeIds(page)).toEqual([1]);
  // Nothing keeps running in the background.
  const [step] = await stepOf(page);
  await page.waitForTimeout(1500);
  expect((await stepOf(page))[0]).toBe(step);
});

test('reset during playback clears the structure and stops the playback', async ({ page }) => {
  await open(page, 'avl');
  await page.locator('#batch-values').fill('5, 4, 3, 2, 1');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await expect(statusLine(page)).toContainText('Playing');
  await page.getByRole('button', { name: 'Reset', exact: true }).click();
  await expectReady(page);
  expect(await drawnNodeIds(page)).toEqual([]);
  expect(await historyEntries(page)).toEqual([]);
  await expect(page.getByRole('button', { name: 'Play', exact: true })).toBeVisible();
});

test('keyboard: R replays the operation on screen and End jumps to its result', async ({ page }) => {
  await open(page, 'queue');
  await page.locator('#batch-values').fill('4, 5, 6');
  await page.getByRole('button', { name: 'Enqueue all', exact: true }).click();
  await finish(page);
  const [, total] = await stepOf(page);
  await page.locator('h1').click(); // focus outside any text box
  await page.keyboard.press('r');
  await expect(statusLine(page)).toContainText('Playing');
  await page.keyboard.press('End');
  await expectReady(page);
  expect(await stepOf(page)).toEqual([total, total]);
  expect(await page.locator('.history-list .history-request').allTextContents()).toEqual(['enqueue 4', 'enqueue 5', 'enqueue 6']);
});
