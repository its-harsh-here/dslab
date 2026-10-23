import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// docs/architecture.md, section 4.
export default defineConfig({
  root: 'web',
  // The site is served from the GitHub Pages sub-path /dslab/
  // (https://<user>.github.io/dslab/), so every URL, the JS bundle and the
  // .wasm file included, starts with it. `npm run dev` and `npm run preview`
  // use the same path: http://localhost:5173/dslab/ and :4173/dslab/.
  base: '/dslab/',
  plugins: [react()],
  build: {
    outDir: '../dist',
    emptyOutDir: true,
  },
});
