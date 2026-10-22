import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// docs/architecture.md, section 4.
export default defineConfig({
  root: 'web',
  // Relative URLs everywhere, so the same build works at "/" and under a
  // GitHub Pages sub-path such as "/dslab/".
  base: './',
  plugins: [react()],
  build: {
    outDir: '../dist',
    emptyOutDir: true,
  },
});
