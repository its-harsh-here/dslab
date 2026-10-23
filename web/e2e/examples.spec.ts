// Batch insert and seeded random examples.
import { drawnNodeIds, expect, expectReady, expectStepMatchesFrame, open, replay, setSpeed, test, type Request } from './support';

test('batch insert sends one request at a time, in order, and ends on the same tree as C++', async ({ page }) => {
  await open(page, 'bst');
  await setSpeed(page, '4');
  await page.locator('#batch-values').fill('5, 3, 8, 1, 4, 3');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await expectReady(page); // played to the end, one operation after another
  expect(await page.locator('.history-list .history-request').allTextContents()).toEqual([
    'insert 5', 'insert 3', 'insert 8', 'insert 1', 'insert 4', 'insert 3',
  ]);
  // The repeated 3 was rejected by C++ and logged as such.
  await expect(page.locator('.history-list li').last()).toContainText('DUPLICATE_VALUE');

  const requests: Request[] = [5, 3, 8, 1, 4, 3].map((value) => ({ structure: 'bst', operation: 'insert', args: { value } }));
  const responses = await replay(page, requests);
  const last = responses.at(-1)!;
  await expectStepMatchesFrame(page, last.frames.at(-1)!, last.frames.length - 1, last.frames.length, 'tree');
});

test('an invalid batch is refused before anything is sent', async ({ page }) => {
  await open(page, 'stack');
  await page.locator('#batch-values').fill('1, two, 3');
  await page.getByRole('button', { name: 'Push all', exact: true }).click();
  await expect(page.locator('#batch-error')).toContainText('whole numbers separated by commas');
  await expect(page.locator('.history-list')).toHaveCount(0);
  expect(await drawnNodeIds(page)).toEqual([]);
});

test('the same seed builds the same random example', async ({ page }) => {
  await open(page, 'redBlack');
  const build = async () => {
    await page.locator('#random-seed').fill('7');
    await page.locator('#random-size').fill('9');
    await page.getByRole('button', { name: 'Build', exact: true }).click();
    await expectReady(page);
    return page.locator('.stage [data-node-id]').evaluateAll((nodes) =>
      nodes
        .map((node) => ({ value: node.querySelector('.node-value')?.textContent, color: node.getAttribute('data-color'), at: node.getAttribute('transform') }))
        .sort((a, b) => Number(a.value) - Number(b.value)),
    );
  };
  const first = await build();
  const second = await build();
  expect(first).toHaveLength(9);
  expect(second).toEqual(first); // same values, colors and positions
});

test('a batch continues past a value C++ rejects without frames (out of the 32-bit range)', async ({ page }) => {
  await open(page, 'bst');
  await setSpeed(page, '4');
  await page.locator('#batch-values').fill('1, 9999999999, 2');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await expectReady(page);
  expect(await page.locator('.history-list .history-request').allTextContents()).toEqual(['insert 1', 'insert 9999999999', 'insert 2']);
  await expect(page.locator('.history-list li').nth(1)).toContainText('VALUE_OUT_OF_RANGE');
  expect(await drawnNodeIds(page)).toEqual([1, 2]);
});
