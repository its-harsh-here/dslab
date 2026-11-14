// Whether the structure list on the left is shown.
//
// Wide screens: the list is a sidebar that the user can hide and show again;
// the choice is remembered in this browser. Narrow screens: the list is a
// drawer over the page, closed until the menu button opens it, and closed
// again once a page is chosen.
import { useCallback, useEffect, useState } from 'react';

const STORAGE_KEY = 'dslab-sidebar';
const NARROW = '(max-width: 899px)';

function storedCollapsed(): boolean {
  try {
    return localStorage.getItem(STORAGE_KEY) === 'collapsed';
  } catch {
    return false; // storage blocked (some private windows): show the sidebar
  }
}

function useMediaQuery(query: string): boolean {
  const [matches, setMatches] = useState(() => window.matchMedia(query).matches);
  useEffect(() => {
    const list = window.matchMedia(query);
    const onChange = () => setMatches(list.matches);
    list.addEventListener('change', onChange);
    return () => list.removeEventListener('change', onChange);
  }, [query]);
  return matches;
}

export interface Sidebar {
  /** True when the list is on screen. */
  readonly open: boolean;
  /** True on narrow screens, where the list is a drawer over the page. */
  readonly narrow: boolean;
  toggle(): void;
  /** Closes the drawer on a narrow screen; does nothing to the wide sidebar. */
  closeDrawer(): void;
}

export function useSidebar(): Sidebar {
  const narrow = useMediaQuery(NARROW);
  const [collapsed, setCollapsed] = useState(storedCollapsed);
  const [drawerOpen, setDrawerOpen] = useState(false);

  const toggle = useCallback(() => {
    if (window.matchMedia(NARROW).matches) {
      setDrawerOpen((open) => !open);
      return;
    }
    setCollapsed((was) => {
      try {
        localStorage.setItem(STORAGE_KEY, was ? 'expanded' : 'collapsed');
      } catch {
        // Not remembered, but still toggled.
      }
      return !was;
    });
  }, []);

  const closeDrawer = useCallback(() => setDrawerOpen(false), []);

  // Leaving the narrow layout closes the drawer, so it is not open the next time.
  useEffect(() => {
    if (!narrow) setDrawerOpen(false);
  }, [narrow]);

  // Ctrl+B (Cmd+B on a Mac) shows or hides the list; Esc closes the drawer.
  useEffect(() => {
    function onKeyDown(event: KeyboardEvent) {
      const target = event.target instanceof HTMLElement ? event.target : null;
      const typing = target?.closest('input, textarea, select, [contenteditable="true"]') != null;
      if ((event.ctrlKey || event.metaKey) && !event.altKey && !event.shiftKey && event.key.toLowerCase() === 'b' && !typing) {
        event.preventDefault();
        toggle();
      } else if (event.key === 'Escape' && drawerOpen) {
        setDrawerOpen(false);
      }
    }
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [toggle, drawerOpen]);

  return { open: narrow ? drawerOpen : !collapsed, narrow, toggle, closeDrawer };
}
