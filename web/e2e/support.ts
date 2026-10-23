// Shared pieces of the end-to-end tests.
//
// - `test` fails any test in which the page logs a console error, throws, or a
//   .wasm request fails or answers with an HTTP error.
// - `replay` runs requests on a fresh, separate C++ engine of the same Wasm
//   module (window.__dslab, see web/src/engine/loadEngine.ts) to get the frames
//   the page should be showing, independently of the page's own state.
// - `expectStepMatchesFrame` compares what is drawn and written on screen with
//   one frame: nodes, edges, labels, highlights, pointers, explanation, output.
import { expect, test as base, type Page } from '@playwright/test';

export { expect };

export const test = base.extend<{ problems: string[] }>({
  problems: [
    async ({ page }, use) => {
      const problems: string[] = [];
      page.on('console', (message) => {
        if (message.type() === 'error') problems.push(`console error: ${message.text()}`);
      });
      page.on('pageerror', (error) => problems.push(`uncaught error: ${error.message}`));
      page.on('requestfailed', (request) => {
        if (request.url().includes('.wasm')) problems.push(`.wasm request failed: ${request.url()} (${request.failure()?.errorText})`);
      });
      page.on('response', (response) => {
        if (response.url().includes('.wasm') && !response.ok()) {
          problems.push(`.wasm request answered HTTP ${response.status()}: ${response.url()}`);
        }
      });
      await use(problems);
      expect(problems, 'the page reported problems').toEqual([]);
    },
    { auto: true },
  ],
});

// --- Types of what C++ sends (only the parts the tests read) -----------------

export interface NodeJson {
  id: number;
  value: number;
  next?: number | null;
  left?: number | null;
  right?: number | null;
  height?: number;
  balance?: number;
  color?: string;
}

export interface FrameJson {
  index: number;
  event: string;
  stepId: string;
  explanation: string;
  highlight: { nodes: number[]; edges: string[] };
  pointers: Record<string, number | null>;
  output: number[];
  pending: number[];
  snapshot: { structure: string; size: number; nodes: NodeJson[] };
}

export interface ResponseJson {
  result: { status: string; code?: string; message?: string; data?: Record<string, unknown> };
  snapshot: { structure: string; size: number; nodes: NodeJson[] } | null;
  frames: FrameJson[];
  history: { undo: number; redo: number } | null;
}

export interface Request {
  structure: string;
  operation: string;
  args?: Record<string, unknown>;
}

// --- Page helpers ----------------------------------------------------------

/** Opens one structure's page and waits for the engine. */
export async function open(page: Page, structure: string) {
  await page.goto(`/#${structure}`);
  await expect(page.locator('.workspace')).toBeVisible();
  await expect(statusLine(page)).toContainText('Ready');
}

export function statusLine(page: Page) {
  return page.locator('.status-line');
}

/** Waits until no operation is playing or waiting. */
export async function expectReady(page: Page) {
  await expect(statusLine(page)).toContainText('Ready', { timeout: 60_000 });
}

/** Clicks Jump to result if an operation is on screen, then waits until ready. */
export async function finish(page: Page) {
  const jump = page.locator('.status-line button', { hasText: 'Jump to result' });
  if (await jump.isVisible()) await jump.click();
  await expectReady(page);
}

export async function setSpeed(page: Page, speed: '0.5' | '1' | '2' | '4') {
  await page.locator('.speed select').selectOption(speed);
}

/** The step indicator, as [shown step (1-based), total]. */
export async function stepOf(page: Page): Promise<[number, number]> {
  const text = (await page.locator('.step-count').textContent()) ?? '';
  const match = /Step (\d+) of (\d+)/.exec(text);
  if (!match) throw new Error(`no step indicator in "${text}"`);
  return [Number(match[1]), Number(match[2])];
}

/** Shows step `index` (0-based) with the timeline slider, as a user dragging it would. */
export async function seek(page: Page, index: number) {
  await page.locator('.timeline input[type=range]').fill(String(index));
  await expect(page.locator('.step-count')).toContainText(`Step ${index + 1} of`);
}

/** Runs requests on a fresh, separate engine of the page's Wasm module; returns every response. */
export async function replay(page: Page, requests: readonly Request[]): Promise<ResponseJson[]> {
  return page.evaluate((list) => {
    const hook = (window as unknown as { __dslab: { createEngine(): { handle(text: string): string; delete(): void } } }).__dslab;
    const engine = hook.createEngine();
    const responses = list.map((request, index) =>
      JSON.parse(engine.handle(JSON.stringify({ schemaVersion: 1, requestId: index + 1, args: {}, ...request }))),
    );
    engine.delete();
    return responses;
  }, requests);
}

/** The node IDs drawn on the stage, sorted. */
export async function drawnNodeIds(page: Page): Promise<number[]> {
  const ids = await page.locator('.stage [data-node-id]').evaluateAll((elements) =>
    elements.map((element) => Number(element.getAttribute('data-node-id'))),
  );
  return ids.sort((a, b) => a - b);
}

// --- Comparing the screen with a frame -------------------------------------

const EVENT_WORDS: Record<string, string> = { notFound: 'not found', fixupCase: 'fix-up case' };

/** What the page shows right now, read from the DOM. */
async function screenState(page: Page) {
  return page.evaluate(() => {
    const translate = (element: Element) => {
      const match = /translate\(([-\d.e]+)[ ,]+([-\d.e]+)\)/.exec(element.getAttribute('transform') ?? '');
      return match ? { x: Number(match[1]), y: Number(match[2]) } : null;
    };
    const nodes = [...document.querySelectorAll('.stage [data-node-id]')].map((element) => ({
      id: Number(element.getAttribute('data-node-id')),
      value: element.querySelector('.node-value')?.textContent ?? '',
      idText: element.querySelector('.node-id')?.textContent ?? '',
      color: element.getAttribute('data-color'),
      badge: element.querySelector('.rb-badge text')?.textContent ?? null,
      avl: element.querySelector('[data-avl-label]')?.textContent ?? null,
      highlighted: element.classList.contains('highlighted'),
      at: translate(element),
    }));
    const edges = [...document.querySelectorAll('.stage [data-edge-id]')].map((element) => ({
      id: element.getAttribute('data-edge-id') ?? '',
      highlighted: element.classList.contains('highlighted'),
      x1: Number(element.getAttribute('x1')),
      y1: Number(element.getAttribute('y1')),
      x2: Number(element.getAttribute('x2')),
      y2: Number(element.getAttribute('y2')),
    }));
    const explanationPanel = document.querySelector('section[aria-labelledby="explanation-heading"]');
    const outputBlock = [...(explanationPanel?.querySelectorAll('.output') ?? [])].find((block) =>
      block.querySelector('.label')?.textContent?.startsWith('Output'),
    );
    return {
      nodes,
      edges,
      pointers: [...document.querySelectorAll('.stage .pointer-label')].map((element) => element.textContent ?? ''),
      explanation: explanationPanel?.querySelector('.explanation')?.textContent ?? '',
      meta: explanationPanel?.querySelector('.meta')?.textContent ?? '',
      output: [...(outputBlock?.querySelectorAll('.value-list li') ?? [])].map((element) => element.textContent ?? ''),
    };
  });
}

type Shape = 'tree' | 'stack' | 'chain';

/**
 * Asserts that the step on screen is exactly `frame` (step `index` of `total`):
 * the same nodes with their values, IDs, colors and AVL labels; the same edges;
 * the highlighted nodes and edges; the pointer labels; the explanation and step
 * line; the traversal output; and every edge drawn between the drawn positions
 * of its two nodes (so nodes and edges come from the same frame).
 */
export async function expectStepMatchesFrame(page: Page, frame: FrameJson, index: number, total: number, shape: Shape) {
  const screen = await screenState(page);
  const snapshotIds = frame.snapshot.nodes.map((node) => node.id).sort((a, b) => a - b);
  expect(screen.nodes.map((node) => node.id).sort((a, b) => a - b), 'drawn nodes').toEqual(snapshotIds);

  for (const node of frame.snapshot.nodes) {
    const drawn = screen.nodes.find((candidate) => candidate.id === node.id)!;
    expect(drawn.value, `value of node ${node.id}`).toBe(String(node.value));
    expect(drawn.idText, `ID label of node ${node.id}`).toBe(`#${node.id}`);
    if (node.color !== undefined) {
      expect(drawn.color, `color of node ${node.id}`).toBe(node.color);
      expect(drawn.badge, `R/B badge of node ${node.id}`).toBe(node.color === 'red' ? 'R' : 'B');
    }
    if (node.height !== undefined) {
      const balance = node.balance! > 0 ? `+${node.balance}` : String(node.balance);
      expect(drawn.avl, `AVL label of node ${node.id}`).toBe(`h${node.height} · bf ${balance}`);
    }
  }

  const links: string[] = [];
  for (const node of frame.snapshot.nodes) {
    for (const target of [node.next, node.left, node.right]) {
      if (target !== null && target !== undefined) links.push(`${node.id}->${target}`);
    }
  }
  expect(screen.edges.map((edge) => edge.id).sort(), 'drawn edges').toEqual(links.sort());

  const hotNodes = [...new Set(frame.highlight.nodes.filter((id) => snapshotIds.includes(id)))].sort((a, b) => a - b);
  expect(screen.nodes.filter((node) => node.highlighted).map((node) => node.id).sort((a, b) => a - b), 'highlighted nodes').toEqual(hotNodes);
  expect(screen.edges.filter((edge) => edge.highlighted).map((edge) => edge.id).sort(), 'highlighted edges').toEqual(
    [...new Set(frame.highlight.edges)].sort(),
  );

  const pointers = Object.entries(frame.pointers)
    .filter(([, id]) => id !== null && snapshotIds.includes(id))
    .map(([name]) => name)
    .sort();
  expect(screen.pointers.sort(), 'pointer labels').toEqual(pointers);

  expect(screen.explanation, 'explanation').toBe(frame.explanation);
  expect(screen.meta, 'step line').toBe(`Step ${index + 1} of ${total} · ${EVENT_WORDS[frame.event] ?? frame.event}`);
  expect(screen.output, 'traversal output').toEqual(frame.output.map(String));

  // Every edge starts and ends at the drawn positions of its two nodes.
  for (const edge of screen.edges) {
    const [from, to] = edge.id.split('->').map(Number);
    const a = screen.nodes.find((node) => node.id === from)!.at!;
    const b = screen.nodes.find((node) => node.id === to)!.at!;
    const near = (x: number, y: number, ex: number, ey: number) => Math.hypot(x - ex, y - ey) < 0.5;
    if (shape === 'tree') {
      expect(Math.abs(Math.hypot(edge.x1 - a.x, edge.y1 - a.y) - 22), `edge ${edge.id} leaves its parent`).toBeLessThan(0.5);
      expect(Math.abs(Math.hypot(edge.x2 - b.x, edge.y2 - b.y) - 24), `edge ${edge.id} reaches its child`).toBeLessThan(0.5);
    } else if (shape === 'stack') {
      expect(near(edge.x1, edge.y1, a.x + 60, a.y + 40) && near(edge.x2, edge.y2, b.x + 60, b.y - 2), `edge ${edge.id}`).toBe(true);
    } else {
      expect(near(edge.x1, edge.y1, a.x + 96, a.y + 20) && near(edge.x2, edge.y2, b.x - 2, b.y + 20), `edge ${edge.id}`).toBe(true);
    }
  }
}

/**
 * Asserts that the highlighted pseudocode line is the catalog line whose step
 * ID is the frame's (no line for the shared "common." steps).
 */
export async function expectPseudocodeLine(page: Page, structure: string, operation: string, frame: FrameJson) {
  const [catalog] = await replay(page, [{ structure, operation: 'catalog' }]);
  const listings = (catalog.result.data as { operations: { operation: string; pseudocode: { stepId: string | null; text: string }[] }[] }).operations;
  const listing = listings.find((entry) => entry.operation === operation);
  const line = listing?.pseudocode.find((entry) => entry.stepId === frame.stepId);
  const current = page.locator('.pseudocode li.current');
  if (frame.stepId.startsWith('common.')) {
    await expect(current).toHaveCount(0);
  } else {
    expect(line, `the catalog lists ${frame.stepId}`).toBeTruthy();
    await expect(current).toHaveCount(1);
    expect(((await current.textContent()) ?? '').trim()).toBe(line!.text.trim());
  }
}

/** Asserts every step of `response` on screen, moving through them with the timeline. */
export async function expectEveryStep(page: Page, response: ResponseJson, shape: Shape) {
  const total = response.frames.length;
  const [, shownTotal] = await stepOf(page);
  expect(shownTotal, 'number of steps').toBe(total);
  const structure = response.frames[0].snapshot.structure;
  const operation = (response as unknown as { operation: string }).operation;
  for (let index = 0; index < total; index++) {
    await seek(page, index);
    await expectStepMatchesFrame(page, response.frames[index], index, total, shape);
    await expectPseudocodeLine(page, structure, operation, response.frames[index]);
  }
}
