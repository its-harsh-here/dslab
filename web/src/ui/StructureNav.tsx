// The structure list: a sidebar on wide screens (it can be hidden), a drawer
// over the page on narrow ones. "Overview" leads back to the start page.
import { STRUCTURES, type StructureName } from '../engine/protocol';
import { GROUPS, STRUCTURE_INFO } from './structures';

/** What the page shows: the overview, or one structure. */
export type View = StructureName | 'home';

export function StructureNav({ active, onChoose, hidden }: {
  active: View;
  onChoose: (view: View) => void;
  /** Off screen: also taken out of the tab order and the accessibility tree. */
  hidden: boolean;
}) {
  return (
    <nav id="structure-nav" className="structure-nav" aria-label="Data structures" inert={hidden}>
      <button type="button" className="nav-item" aria-current={active === 'home' ? 'page' : undefined} onClick={() => onChoose('home')}>
        <svg className="ds-icon" viewBox="0 0 24 24" aria-hidden="true" focusable="false">
          <rect x={4} y={4} width={6.5} height={6.5} rx={1.5} />
          <rect x={13.5} y={4} width={6.5} height={6.5} rx={1.5} />
          <rect x={4} y={13.5} width={6.5} height={6.5} rx={1.5} />
          <rect x={13.5} y={13.5} width={6.5} height={6.5} rx={1.5} />
        </svg>
        Overview
      </button>
      {GROUPS.map((group) => (
        <div className="nav-group" key={group}>
          <p className="nav-heading" id={`nav-${group.replace(/\W+/g, '-').toLowerCase()}`}>{group}</p>
          <ul aria-labelledby={`nav-${group.replace(/\W+/g, '-').toLowerCase()}`}>
            {STRUCTURES.filter((structure) => STRUCTURE_INFO[structure].group === group).map((structure) => (
              <li key={structure}>
                <button type="button" className="nav-item" aria-current={structure === active ? 'page' : undefined}
                  onClick={() => onChoose(structure)}>
                  {STRUCTURE_INFO[structure].icon}
                  {STRUCTURE_INFO[structure].label}
                </button>
              </li>
            ))}
          </ul>
        </div>
      ))}
    </nav>
  );
}
