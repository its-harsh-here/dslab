// The deque, doubly linked list, circular list, heaps, splay tree and hash
// table: every step of an operation on screen matches the frame C++ recorded
// for it (nodes, values, IDs, next and prev links, highlights, pointer labels,
// explanation, step line, pseudocode line), plus what only these pages draw:
// the heap's array, the hash table's lit bucket, the ring. And the overview
// page that leads to all fourteen structures.
// Tagged @smoke: CI also runs these before deploying (npm run test:e2e:smoke).
import { expect, expectEveryStep, expectReady, finish, open, replay, seek, statusLine, test, type Request } from './support';

async function batch(page: import('@playwright/test').Page, values: string, button: string) {
  await page.locator('#batch-values').fill(values);
  await page.getByRole('button', { name: button, exact: true }).click();
  await finish(page);
}

const each = (structure: string, operation: string, values: number[]): Request[] =>
  values.map((value) => ({ structure, operation, args: { value } }));

test('overview: fourteen structures, and a card opens its page', { tag: '@smoke' }, async ({ page }) => {
  await page.goto('./'); // no structure in the URL: the overview
  await expect(page.locator('.home .card')).toHaveCount(14);
  await expect(page.locator('.structure-nav li')).toHaveCount(14);
  await page.locator('.home .card', { hasText: 'Splay tree' }).click();
  await expect(page.locator('.workspace h2')).toHaveText('Splay tree');
  await expect(statusLine(page)).toContainText('Ready');
  expect(new URL(page.url()).hash).toBe('#splay');
  await page.locator('.structure-nav button', { hasText: 'Overview' }).click();
  await expect(page.locator('.home .hero')).toBeVisible();
});

test('deque: every step of a pop at the rear, with prev links drawn', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'deque');
  await batch(page, '10, 20, 30', 'Push back all');
  await page.locator('#deque-value').fill('5');
  await page.getByRole('button', { name: 'Push front', exact: true }).click();
  await finish(page);
  await page.getByRole('button', { name: 'Pop back', exact: true }).click();
  const responses = await replay(page, [
    ...each('deque', 'pushBack', [10, 20, 30]),
    { structure: 'deque', operation: 'pushFront', args: { value: 5 } },
    { structure: 'deque', operation: 'popBack' },
  ]);
  await expectEveryStep(page, responses.at(-1)!, 'free');
  // Three nodes are left: two next links and two prev links.
  await expect(page.locator('.stage [data-edge-id]')).toHaveCount(4);
  await expect(page.locator('.stage [data-edge-id][data-link="prev"]')).toHaveCount(2);
});

test('doubly linked list: every step of an insert near the tail, then a delete without a walk', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'doublyList');
  await batch(page, '10, 20, 30, 40, 50', 'Append all');
  await page.locator('#doublylist-value').fill('45');
  await page.locator('#doublylist-index').fill('4');
  await page.getByRole('button', { name: 'Insert at index', exact: true }).click();
  const requests: Request[] = [
    ...[10, 20, 30, 40, 50].map((value, index) => ({ structure: 'doublyList', operation: 'insertAt', args: { index, value } })),
    { structure: 'doublyList', operation: 'insertAt', args: { index: 4, value: 45 } },
  ];
  const inserted = (await replay(page, requests)).at(-1)!;
  expect(inserted.frames.some((frame) => frame.stepId === 'doublyList.insertAt.startTail')).toBe(true);
  await expectEveryStep(page, inserted, 'free');
  await finish(page);

  await page.locator('.stage [data-node-id="3"]').click();
  await page.getByRole('button', { name: 'Delete selected', exact: true }).click();
  const deleted = (await replay(page, [...requests, { structure: 'doublyList', operation: 'deleteNode', args: { nodeId: 3 } }])).at(-1)!;
  expect(deleted.frames.every((frame) => frame.event !== 'move')).toBe(true); // no walk
  await expectEveryStep(page, deleted, 'free');
});

test('circular list: every step of an insert and a rotation on the ring', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'circularList');
  await page.locator('#circularlist-value').fill('7');
  await page.getByRole('button', { name: 'Insert back', exact: true }).click();
  const first = (await replay(page, each('circularList', 'insertBack', [7]))).at(-1)!;
  await expectEveryStep(page, first, 'free'); // a circle of one: the link is a loop
  await finish(page);

  await batch(page, '8, 9, 10', 'Insert back all');
  await page.getByRole('button', { name: 'Rotate', exact: true }).click();
  const requests = [...each('circularList', 'insertBack', [7, 8, 9, 10]), { structure: 'circularList', operation: 'rotate' }];
  const rotated = (await replay(page, requests)).at(-1)!;
  await expectEveryStep(page, rotated, 'free');
  // Four nodes, four links: the last one closes the circle. No NULL is drawn.
  await expect(page.locator('.stage [data-edge-id]')).toHaveCount(4);
  await expect(page.locator('.stage .null-link')).toHaveCount(0);
});

test('min-heap: every step of an extract, in the tree and in the array', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'minHeap');
  await batch(page, '40, 10, 30, 70, 20, 60, 50', 'Insert all');
  await page.getByRole('button', { name: 'Extract min', exact: true }).click();
  const responses = await replay(page, [...each('minHeap', 'insert', [40, 10, 30, 70, 20, 60, 50]), { structure: 'minHeap', operation: 'extract' }]);
  const extract = responses.at(-1)!;
  expect(extract.result.data).toMatchObject({ value: 10 });
  await expectEveryStep(page, extract, 'tree');
  // On every step the array under the tree is the snapshot's nodes by slot.
  for (let index = 0; index < extract.frames.length; index++) {
    await seek(page, index);
    const bySlot = [...extract.frames[index].snapshot.nodes].sort((a, b) => a.index! - b.index!);
    await expect(page.locator('.stage .heap-cell .heap-cell-value')).toHaveText(bySlot.map((node) => String(node.value)));
    for (const node of bySlot) {
      await expect(page.locator(`.stage [data-node-id="${node.id}"] [data-slot-label]`)).toHaveText(`slot ${node.index}`);
    }
  }
});

test('max-heap: every step of an insert that sifts up to the root', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'maxHeap');
  await batch(page, '10, 20, 30', 'Insert all');
  await page.locator('#maxheap-value').fill('99');
  await page.getByRole('button', { name: 'Insert', exact: true }).click();
  const responses = await replay(page, each('maxHeap', 'insert', [10, 20, 30, 99]));
  expect(responses.at(-1)!.frames.filter((frame) => frame.event === 'swap')).toHaveLength(2);
  await expectEveryStep(page, responses.at(-1)!, 'tree');
});

test('splay tree: every step of a search that splays a deep node to the root', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'splay');
  await batch(page, '10, 20, 30, 40, 50', 'Insert all');
  await page.locator('#splaytree-value').fill('10');
  await page.getByRole('button', { name: 'Search', exact: true }).click();
  const responses = await replay(page, [...each('splay', 'insert', [10, 20, 30, 40, 50]), { structure: 'splay', operation: 'search', args: { value: 10 } }]);
  const search = responses.at(-1)!;
  expect(search.frames.some((frame) => frame.stepId === 'splay.search.zigZig')).toBe(true);
  await expectEveryStep(page, search, 'tree');
  // A search changed the tree, so it can be undone.
  await finish(page);
  await expect(page.getByRole('button', { name: 'Undo', exact: true })).toBeEnabled();
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  await expectReady(page);
  const before = responses.at(-2)!.snapshot as unknown as { root: number };
  await expect(page.locator(`.stage [data-node-id="${before.root}"] .node-value`)).toHaveText('50');
});

test('hash table: every step of a delete in a chain, with the bucket lit', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'hashTable');
  await batch(page, '4, 11, 18, 25, 6', 'Insert all');
  await page.locator('#hashtable-value').fill('11');
  await page.getByRole('button', { name: 'Delete', exact: true }).click();
  const responses = await replay(page, [...each('hashTable', 'insert', [4, 11, 18, 25, 6]), { structure: 'hashTable', operation: 'delete', args: { value: 11 } }]);
  const removed = responses.at(-1)!;
  await expectEveryStep(page, removed, 'free');
  await expect(page.locator('.stage .bucket')).toHaveCount(7);
  for (let index = 0; index < removed.frames.length; index++) {
    await seek(page, index);
    const bucket = removed.frames[index].bucket ?? null;
    const lit = page.locator('.stage .bucket.active');
    if (bucket === null) {
      await expect(lit).toHaveCount(0);
    } else {
      await expect(lit).toHaveCount(1);
      await expect(lit).toHaveAttribute('data-bucket', String(bucket)); // 11 mod 7 = 4
    }
  }
  expect(removed.frames.at(-1)!.bucket).toBe(4);
});
