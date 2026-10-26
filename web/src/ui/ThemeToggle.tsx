// The "Dark mode" switch at the top left of the page. Light is the default.
//
// The theme is one attribute on <html>: data-theme="dark" turns on the dark
// colours in styles.css. The choice is saved in this browser (localStorage), and
// index.html applies a saved "dark" before the page is drawn, so a returning
// visitor never sees a flash of the light theme.
import { useState } from 'react';

const STORAGE_KEY = 'dslab-theme'; // the same key as the script in index.html

type Theme = 'light' | 'dark';

/** The theme the page is showing now (index.html may already have set dark). */
function currentTheme(): Theme {
  return document.documentElement.dataset.theme === 'dark' ? 'dark' : 'light';
}

function applyTheme(theme: Theme) {
  document.documentElement.dataset.theme = theme;
  try {
    localStorage.setItem(STORAGE_KEY, theme);
  } catch {
    // Storage can be blocked (some private windows): the theme still changes, it is just not remembered.
  }
}

export function ThemeToggle() {
  const [theme, setTheme] = useState<Theme>(currentTheme);
  const dark = theme === 'dark';

  function toggle() {
    const next: Theme = dark ? 'light' : 'dark';
    applyTheme(next);
    setTheme(next);
  }

  // aria-pressed makes it a toggle button: screen readers say "Dark mode, pressed" when dark is on.
  return (
    <button type="button" className="theme-toggle" aria-pressed={dark} onClick={toggle}>
      <svg viewBox="0 0 24 24" aria-hidden="true" focusable="false">
        <path d="M20 14.5A8.5 8.5 0 1 1 9.5 4 7.5 7.5 0 0 0 20 14.5z" />
      </svg>
      Dark mode
    </button>
  );
}
