// Every step of an operation, as shown on screen, matches the frame C++
// recorded for it: nodes, values, IDs, colors, AVL labels, edges, highlights,
// pointer labels, explanation, step line and traversal output. The expected
// frames come from replaying the same requests on a separate engine.
import { expectEveryStep, finish, open, replay, test, type Request } from './support';

async function batch(page: import('@playwright/test').Page, values: string, button: string) {
  await page.locator('#batch-values').fill(values);
  await page.getByRole('button', { name: button, exact: true }).click();
  await finish(page);
}

const inserts = (structure: string, values: number[], operation = 'insert'): Request[] =>
  values.map((value) => ({ structure, operation, args: { value } }));

test('stack: every step of a pop', async ({ page }) => {
  await open(page, 'stack');
  await batch(page, '10, 20, 30', 'Push all');
  await page.getByRole('button', { name: 'Pop', exact: true }).click();
  const responses = await replay(page, [...inserts('stack', [10, 20, 30], 'push'), { structure: 'stack', operation: 'pop' }]);
  await expectEveryStep(page, responses.at(-1)!, 'stack');
});

test('queue: every step of a dequeue', async ({ page }) => {
  await open(page, 'queue');
  await batch(page, '1, 2, 3', 'Enqueue all');
  await page.getByRole('button', { name: 'Dequeue', exact: true }).click();
  const responses = await replay(page, [...inserts('queue', [1, 2, 3], 'enqueue'), { structure: 'queue', operation: 'dequeue' }]);
  await expectEveryStep(page, responses.at(-1)!, 'chain');
});

test('list: every step of an insert in the middle', async ({ page }) => {
  await open(page, 'list');
  const requests: Request[] = [];
  for (const [index, value] of [[0, 5], [1, 7], [1, 6]]) {
    await page.locator('#list-value').fill(String(value));
    await page.locator('#list-index').fill(String(index));
    await page.getByRole('button', { name: 'Insert at index', exact: true }).click();
    requests.push({ structure: 'list', operation: 'insertAt', args: { index, value } });
    if (requests.length < 3) await finish(page);
  }
  const responses = await replay(page, requests);
  await expectEveryStep(page, responses.at(-1)!, 'chain');
});

test('binary tree: every step of an in-order traversal, with the output building up', async ({ page }) => {
  await open(page, 'binaryTree');
  await page.locator('#tree-value').fill('1');
  await page.getByRole('button', { name: 'Add root', exact: true }).click();
  await finish(page);
  const addChild = async (parent: number, side: 'left' | 'right', value: number) => {
    await page.locator('#tree-value').fill(String(value));
    await page.getByRole('button', { name: `Add ${side} child`, exact: true }).click();
    await finish(page);
  };
  await page.locator('.stage [data-node-id="1"]').click(); // select node 1
  await addChild(1, 'left', 2);
  await addChild(1, 'right', 3);
  await page.locator('.stage [data-node-id="2"]').click(); // select node 2
  await addChild(2, 'left', 4);
  await page.getByRole('button', { name: 'In-order', exact: true }).click();
  const responses = await replay(page, [
    { structure: 'binaryTree', operation: 'addRoot', args: { value: 1 } },
    { structure: 'binaryTree', operation: 'addChild', args: { parentId: 1, side: 'left', value: 2 } },
    { structure: 'binaryTree', operation: 'addChild', args: { parentId: 1, side: 'right', value: 3 } },
    { structure: 'binaryTree', operation: 'addChild', args: { parentId: 2, side: 'left', value: 4 } },
    { structure: 'binaryTree', operation: 'inorder' },
  ]);
  await expectEveryStep(page, responses.at(-1)!, 'tree');
});

test('BST: every step of a delete with a deeper successor', async ({ page }) => {
  await open(page, 'bst');
  await batch(page, '50, 30, 70, 20, 40, 60, 80, 65', 'Insert all');
  await page.locator('#binarysearchtree-value').fill('50');
  await page.getByRole('button', { name: 'Delete', exact: true }).click();
  const responses = await replay(page, [...inserts('bst', [50, 30, 70, 20, 40, 60, 80, 65]), { structure: 'bst', operation: 'delete', args: { value: 50 } }]);
  await expectEveryStep(page, responses.at(-1)!, 'tree');
});

test('AVL: every step of an LR rotation, with heights and balances', async ({ page }) => {
  await open(page, 'avl');
  await batch(page, '30, 10', 'Insert all');
  await page.locator('#avltree-value').fill('20');
  await page.getByRole('button', { name: 'Insert', exact: true }).click();
  const responses = await replay(page, inserts('avl', [30, 10, 20]));
  await expectEveryStep(page, responses.at(-1)!, 'tree');
});

test('red-black: every step of a delete (cases 3 and 4), with colors and NIL leaves shown', async ({ page }) => {
  await open(page, 'redBlack');
  await page.getByLabel('Show NIL leaves').check();
  await batch(page, '10, 20, 40, 30', 'Insert all');
  await page.locator('#redblacktree-value').fill('10');
  await page.getByRole('button', { name: 'Delete', exact: true }).click();
  const responses = await replay(page, [...inserts('redBlack', [10, 20, 40, 30]), { structure: 'redBlack', operation: 'delete', args: { value: 10 } }]);
  await expectEveryStep(page, responses.at(-1)!, 'tree');
});
