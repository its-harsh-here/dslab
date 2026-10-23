// Deployment smoke test. CI runs every test tagged @smoke (this one and the
// seven in frames.spec.ts) against `npm run preview` before it deploys: the
// page, its JavaScript and the .wasm engine must all load from under /dslab/,
// the path GitHub Pages serves the site from.
import { expect, open, test } from './support';

test('the page, its JavaScript and the .wasm file load under /dslab/', { tag: '@smoke' }, async ({ page }) => {
  const loaded: string[] = [];
  page.on('response', (response) => {
    if (response.ok()) loaded.push(new URL(response.url()).pathname);
  });
  await open(page, 'stack'); // waits until the C++ engine is ready

  expect(new URL(page.url()).pathname).toBe('/dslab/');
  const scripts = loaded.filter((path) => path.endsWith('.js'));
  const wasm = loaded.filter((path) => path.endsWith('.wasm'));
  expect(scripts.length, 'JavaScript files loaded').toBeGreaterThan(0);
  expect(wasm, '.wasm files loaded').toHaveLength(1);
  for (const path of [...scripts, ...wasm]) expect(path).toMatch(/^\/dslab\/assets\//);
});
