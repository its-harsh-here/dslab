// End-to-end tests (npm run test:e2e) against the production build served by
// `npm run preview`, in Playwright's bundled Chromium.
//
// Never set channel "chrome": Google Chrome does not exist for Linux ARM64.
// channel "chromium" is Playwright's own bundled Chromium (chromium-1248 for
// @playwright/test 1.64.1) in its new headless mode, so the separate
// "headless shell" download is not needed.
import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: 'web/e2e',
  // One worker: the animations are timed, and the machine is shared with the build.
  workers: 1,
  timeout: 90_000,
  expect: { timeout: 10_000 },
  reporter: [['list']],
  outputDir: 'test-results',
  use: {
    // The same base path as the deployed site (vite.config.ts).
    baseURL: 'http://localhost:4173/dslab/',
    browserName: 'chromium',
    channel: 'chromium',
    viewport: { width: 1440, height: 900 },
    trace: 'retain-on-failure',
  },
  webServer: {
    command: 'npm run preview -- --port 4173 --strictPort',
    url: 'http://localhost:4173/dslab/',
    reuseExistingServer: false,
    timeout: 60_000,
  },
});
