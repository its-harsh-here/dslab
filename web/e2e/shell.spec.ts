// The application shell: the sidebar that can be hidden, the drawer on a
// phone, browser history and tab titles, the visible wording, and that the
// analytics container is never loaded on localhost.
import { expect, expectReady, open, statusLine, test } from './support';

test('the sidebar hides and shows with its button and Ctrl+B, and the choice is remembered', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'stack');
  const nav = page.locator('#structure-nav');
  const toggle = page.locator('.sidebar-toggle');
  const stageWidth = async () => (await page.locator('.stage').boundingBox())!.width;
  await expect(nav).toBeVisible();
  await expect(toggle).toHaveAttribute('aria-expanded', 'true');
  const narrower = await stageWidth();

  await toggle.click();
  await expect(toggle).toHaveAttribute('aria-expanded', 'false');
  await expect(nav).toBeHidden();
  await expect.poll(stageWidth).toBeGreaterThan(narrower + 150); // the page takes the room

  await page.reload();
  await expect(statusLine(page)).toContainText('Ready');
  await expect(nav).toBeHidden(); // remembered

  await page.locator('h1').click(); // focus outside any text box
  await page.keyboard.press('Control+b');
  await expect(nav).toBeVisible();
  await expect(toggle).toHaveAttribute('aria-expanded', 'true');
  await page.keyboard.press('Control+b');
  await expect(nav).toBeHidden();

  // Typing in a box never toggles it.
  await page.locator('#stack-value').click();
  await page.keyboard.press('Control+b');
  await expect(nav).toBeHidden();
});

test('phone: the structure list is a drawer that closes after choosing', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await open(page, 'stack');
  const nav = page.locator('#structure-nav');
  await expect(nav).toBeHidden();
  await page.locator('.sidebar-toggle').click();
  await expect(nav).toBeVisible();
  await nav.locator('button', { hasText: 'Hash table' }).click();
  await expect(nav).toBeHidden();
  await expect(page.locator('.workspace h2')).toHaveText('Hash table (separate chaining)');
  // Esc and the backdrop close it too.
  await page.locator('.sidebar-toggle').click();
  await page.keyboard.press('Escape');
  await expect(nav).toBeHidden();
  await page.locator('.sidebar-toggle').click();
  await page.locator('.drawer-backdrop').click({ position: { x: 370, y: 400 } });
  await expect(nav).toBeHidden();
  expect(await page.evaluate(() => document.documentElement.scrollWidth)).toBeLessThanOrEqual(390);
});

test('Back and Forward move between pages, and the tab title names the page', async ({ page }) => {
  await page.goto('./');
  await expect(page).toHaveTitle('DSLab · Data structure visualizer');
  await page.locator('.home .card', { hasText: 'AVL tree' }).click();
  await expectReady(page);
  await expect(page).toHaveTitle('AVL tree · DSLab');
  await page.locator('#structure-nav button', { hasText: 'Min-heap' }).click();
  await expect(page).toHaveTitle('Min-heap · DSLab');
  await page.goBack();
  await expect(page).toHaveTitle('AVL tree · DSLab');
  await expect(page.locator('.workspace h2')).toHaveText('AVL tree');
  await page.goBack();
  await expect(page.locator('.home .hero')).toBeVisible();
  await page.goForward();
  await expect(page.locator('.workspace h2')).toHaveText('AVL tree');
});

test('the visible text never mentions C++, and no analytics request is made on localhost', async ({ page }) => {
  const external: string[] = [];
  page.on('request', (request) => {
    if (!request.url().startsWith('http://localhost')) external.push(request.url());
  });
  await page.goto('./');
  await expect(page.locator('.home .card')).toHaveCount(14);
  expect(await page.locator('body').innerText()).not.toContain('C++');
  await open(page, 'redBlack');
  expect(await page.locator('body').innerText()).not.toContain('C++');
  expect(external).toEqual([]);
  expect(await page.evaluate(() => 'dataLayer' in window)).toBe(false);
});

test('the logo reacts to a click and to playback, and so does the tab icon', async ({ page }) => {
  await open(page, 'bst');
  const mark = page.locator('.brand-mark');
  const icon = page.locator('link[rel="icon"]');
  const original = await icon.getAttribute('href');
  expect(original).toBe('/dslab/favicon.svg');

  // A click spins the tree and the tab icon, then both settle again.
  await page.locator('.brand-button').click();
  await expect(mark).toHaveClass(/celebrate/);
  await expect(icon).toHaveAttribute('href', /^data:image\/svg\+xml,/);
  await expect(mark).not.toHaveClass(/celebrate/);
  await expect(icon).toHaveAttribute('href', original!);

  // While an operation plays, both show activity; afterwards they are idle again.
  await page.locator('#batch-values').fill('50, 30, 70, 20, 40');
  await page.getByRole('button', { name: 'Insert all', exact: true }).click();
  await expect(mark).toHaveClass(/busy/);
  await expect(icon).toHaveAttribute('href', /^data:image\/svg\+xml,/);
  await page.locator('.status-line button', { hasText: 'Jump to result' }).click();
  await expectReady(page);
  await expect(mark).not.toHaveClass(/busy/);
  await expect(icon).toHaveAttribute('href', original!);
});
