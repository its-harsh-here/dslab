// Interaction details: reduced motion, pan and zoom versus clicking nodes, and
// the narrow (phone) layout.
import { expect, expectEveryStep, expectReady, finish, open, replay, setSpeed, test, type Request } from './support';

const avlInserts = (values: number[]): Request[] => values.map((value) => ({ structure: 'avl', operation: 'insert', args: { value } }));

test('with reduced motion, operations still play step by step and match their frames', async ({ page }) => {
  await page.emulateMedia({ reducedMotion: 'reduce' });
  await open(page, 'avl');
  await setSpeed(page, '4');
  await page.locator('#batch-values').fill('30, 10, 20');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await expectReady(page); // played to the end without movement
  const responses = await replay(page, avlInserts([30, 10, 20]));
  await expectEveryStep(page, responses.at(-1)!, 'tree');
});

test('dragging pans and the wheel zooms, and a click afterwards still selects a node', async ({ page }) => {
  await open(page, 'bst');
  await page.locator('#batch-values').fill('50, 30, 70');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);
  const svg = page.locator('.stage svg');
  const viewBox = () => svg.getAttribute('viewBox');
  const before = await viewBox();

  // Drag across empty space on the stage.
  const stage = (await page.locator('.stage').boundingBox())!;
  await page.mouse.move(stage.x + 30, stage.y + stage.height - 30);
  await page.mouse.down();
  await page.mouse.move(stage.x + 130, stage.y + stage.height - 60, { steps: 5 });
  await page.mouse.up();
  const panned = await viewBox();
  expect(panned).not.toBe(before);
  await expect(page.locator('section[aria-labelledby="node-heading"]')).toContainText('Click a node'); // the drag selected nothing

  // A real click after the drag selects the node under the pointer.
  await page.locator('.stage [data-node-id="2"]').click();
  await expect(page.locator('section[aria-labelledby="node-heading"]')).toContainText('#2');
  await expect(page.locator('#binarysearchtree-value')).toHaveValue('30');

  // Zoom with the wheel over the stage, then click another node.
  await page.mouse.move(stage.x + stage.width / 2, stage.y + stage.height / 2);
  await page.mouse.wheel(0, -300);
  await expect.poll(viewBox).not.toBe(panned);
  await page.locator('.stage [data-node-id="3"]').click();
  await expect(page.locator('section[aria-labelledby="node-heading"]')).toContainText('#3');

  // Fit returns to the automatic view.
  await page.getByRole('button', { name: 'Fit', exact: true }).click();
  await expect.poll(viewBox).toBe(before);
});

test('phone width: every step still matches its frame, and nothing overflows sideways', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await open(page, 'redBlack');
  await page.locator('.examples-box summary').click(); // examples start closed on a phone
  await page.locator('#batch-values').fill('10, 20, 40, 30');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await finish(page);
  await page.locator('#redblacktree-value').fill('10');
  await page.getByRole('button', { name: 'Delete', exact: true }).click();
  const requests: Request[] = [10, 20, 40, 30].map((value) => ({ structure: 'redBlack', operation: 'insert', args: { value } }));
  const responses = await replay(page, [...requests, { structure: 'redBlack', operation: 'delete', args: { value: 10 } }]);
  await expectEveryStep(page, responses.at(-1)!, 'tree');
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390);
});
