// The structure selector: grouped lists on wide screens, one scrolling row of
// chips on narrow ones. "Overview" leads back to the start page.
import { useEffect, useRef, type CSSProperties } from 'react';

import { STRUCTURES, type StructureName } from '../engine/protocol';
import { GROUPS, STRUCTURE_INFO } from './structures';

/** What the page shows: the overview, or one structure. */
export type View = StructureName | 'home';

export function StructureNav({ active, onChoose }: { active: View; onChoose: (view: View) => void }) {
  // On a phone the selector is one scrolling row: keep the current entry in view.
  const nav = useRef<HTMLElement>(null);
  useEffect(() => {
    const current = nav.current?.querySelector<HTMLElement>('[aria-current="page"]');
    if (!nav.current || !current) return;
    const left = current.offsetLeft - nav.current.offsetLeft;
    if (left < nav.current.scrollLeft || left + current.offsetWidth > nav.current.scrollLeft + nav.current.clientWidth) {
      nav.current.scrollLeft = Math.max(0, left - nav.current.clientWidth / 3);
    }
  }, [active]);

  return (
    <nav ref={nav} className="structure-nav" aria-label="Data structures">
      <button type="button" className="nav-home" aria-current={active === 'home' ? 'page' : undefined} onClick={() => onChoose('home')}>
        <svg className="ds-icon" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
          <rect x={3} y={3} width={7.5} height={7.5} rx={1.6} />
          <rect x={13.5} y={3} width={7.5} height={7.5} rx={1.6} />
          <rect x={3} y={13.5} width={7.5} height={7.5} rx={1.6} />
          <rect x={13.5} y={13.5} width={7.5} height={7.5} rx={1.6} />
        </svg>
        Overview
      </button>
      {GROUPS.map((group) => (
        <div className="nav-group" key={group}>
          <p className="nav-heading wide-only">{group}</p>
          <ul>
            {STRUCTURES.filter((structure) => STRUCTURE_INFO[structure].group === group).map((structure) => {
              const info = STRUCTURE_INFO[structure];
              return (
                <li key={structure} style={{ '--hue': info.hue } as CSSProperties}>
                  <button type="button" aria-current={structure === active ? 'page' : undefined} onClick={() => onChoose(structure)}>
                    {info.icon}
                    <span className="wide-only">{info.label}</span>
                    <span className="narrow-only">{info.short}</span>
                  </button>
                </li>
              );
            })}
          </ul>
        </div>
      ))}
    </nav>
  );
}
