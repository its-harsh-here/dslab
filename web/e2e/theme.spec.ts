// The theme (web/src/ui/ThemeToggle.tsx): light by default, even when the
// system is set to dark; the "Dark mode" button at the top left switches to
// dark and back; the choice is remembered on the next visit.
// Tagged @smoke: CI also runs it before deploying (npm run test:e2e:smoke).
import { expect, open, statusLine, test } from './support';

// A visitor whose system is set to dark must still get the light theme first.
test.use({ colorScheme: 'dark' });

const WHITE = 'rgb(255, 255, 255)'; // --bg, light
const DARK = 'rgb(13, 17, 23)'; // --bg, dark (#0d1117)

test('light by default; the Dark mode button at the top left switches to dark and back, and is remembered', { tag: '@smoke' }, async ({ page }) => {
  await open(page, 'stack');
  const html = page.locator('html');
  const toggle = page.getByRole('button', { name: 'Dark mode' });
  const background = () => page.evaluate(() => getComputedStyle(document.body).backgroundColor);

  // The button sits in the top-left corner, before the title.
  const box = await toggle.boundingBox();
  expect(box, 'the Dark mode button is on screen').not.toBeNull();
  expect(box!.x).toBeLessThan(32);
  expect(box!.y).toBeLessThan(32);
  await expect(page.locator('.app-header > :first-child')).toHaveText('Dark mode');

  // Light, with the button not pressed.
  await expect(html).not.toHaveAttribute('data-theme', 'dark');
  await expect(toggle).toHaveAttribute('aria-pressed', 'false');
  expect(await background()).toBe(WHITE);

  // Dark on.
  await toggle.click();
  await expect(html).toHaveAttribute('data-theme', 'dark');
  await expect(toggle).toHaveAttribute('aria-pressed', 'true');
  expect(await background()).toBe(DARK);

  // Remembered: the next visit opens in dark.
  await page.reload();
  await expect(statusLine(page)).toContainText('Ready');
  await expect(html).toHaveAttribute('data-theme', 'dark');
  await expect(toggle).toHaveAttribute('aria-pressed', 'true');
  expect(await background()).toBe(DARK);

  // And back to light.
  await toggle.click();
  await expect(html).toHaveAttribute('data-theme', 'light');
  await expect(toggle).toHaveAttribute('aria-pressed', 'false');
  expect(await background()).toBe(WHITE);
});
