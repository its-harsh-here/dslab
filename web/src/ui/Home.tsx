// The overview page: what DSLab is, and one card per structure. Each card's
// small drawing is a decorative loop (CSS only); the real animations on the
// structure pages are played from frames recorded by the C++ engine.
import type { CSSProperties } from 'react';

import { STRUCTURES, type StructureName } from '../engine/protocol';
import { GROUPS, STRUCTURE_INFO } from './structures';

/** The decorative drawing on a card: a few shapes that move in a loop. */
function Preview({ structure }: { structure: StructureName }) {
  const group = STRUCTURE_INFO[structure].group;
  if (structure === 'stack') {
    return (
      <svg viewBox="0 0 120 70" className="preview pv-stack" aria-hidden="true">
        {[0, 1, 2, 3].map((i) => <rect key={i} className={`pv-block b${i}`} x={38} y={52 - i * 14} width={44} height={11} rx={3} />)}
      </svg>
    );
  }
  if (structure === 'hashTable') {
    return (
      <svg viewBox="0 0 120 70" className="preview pv-hash" aria-hidden="true">
        {[0, 1, 2, 3].map((row) => (
          <g key={row}>
            <rect className="pv-cell" x={12} y={8 + row * 15} width={16} height={11} rx={2.5} />
            {Array.from({ length: [2, 0, 3, 1][row] }, (_, i) => (
              <g key={i}>
                <line className="pv-link" x1={28 + i * 26} y1={13.5 + row * 15} x2={38 + i * 26} y2={13.5 + row * 15} />
                <rect className={`pv-block d${(row + i) % 4}`} x={38 + i * 26} y={8 + row * 15} width={16} height={11} rx={5.5} />
              </g>
            ))}
          </g>
        ))}
      </svg>
    );
  }
  if (structure === 'circularList') {
    return (
      <svg viewBox="0 0 120 70" className="preview pv-ring" aria-hidden="true">
        <circle className="pv-link" cx={60} cy={35} r={24} fill="none" />
        <g className="pv-spin">
          {[0, 1, 2, 3, 4].map((i) => {
            const angle = (i * 2 * Math.PI) / 5;
            return <circle key={i} className={`pv-block d${i % 4}`} cx={60 + 24 * Math.cos(angle)} cy={35 + 24 * Math.sin(angle)} r={6.5} />;
          })}
        </g>
      </svg>
    );
  }
  if (group === 'Linear') {
    const doubly = structure === 'deque' || structure === 'doublyList';
    return (
      <svg viewBox="0 0 120 70" className={`preview pv-chain pv-${structure}`} aria-hidden="true">
        {[0, 1, 2, 3].map((i) => (
          <g key={i}>
            {i < 3 && <line className="pv-link flow" x1={30 + i * 26} y1={doubly ? 32 : 35} x2={36 + i * 26} y2={doubly ? 32 : 35} />}
            {i < 3 && doubly && <line className="pv-link flow back" x1={36 + i * 26} y1={38} x2={30 + i * 26} y2={38} />}
            <rect className={`pv-block d${i}`} x={12 + i * 26} y={27} width={18} height={16} rx={4} />
          </g>
        ))}
      </svg>
    );
  }
  // Trees and heaps: seven nodes in three levels.
  const at: [number, number][] = [[60, 12], [34, 34], [86, 34], [20, 58], [48, 58], [72, 58], [100, 58]];
  const links: [number, number][] = [[0, 1], [0, 2], [1, 3], [1, 4], [2, 5], [2, 6]];
  const heap = group === 'Heaps and hashing';
  return (
    <svg viewBox="0 0 120 70" className={`preview pv-tree pv-${structure}`} aria-hidden="true">
      {links.map(([a, b], i) => <line key={i} className={`pv-link l${i}`} x1={at[a][0]} y1={at[a][1]} x2={at[b][0]} y2={at[b][1]} />)}
      {at.map(([x, y], i) => <circle key={i} className={`pv-block n${i}${heap ? ' heap' : ''}`} cx={x} cy={y} r={7} />)}
    </svg>
  );
}

export function Home({ onChoose }: { onChoose: (structure: StructureName) => void }) {
  let order = 0;
  return (
    <div className="home">
      <section className="hero" aria-labelledby="hero-title">
        <p className="eyebrow">An interactive data-structure simulator</p>
        <h2 id="hero-title">
          Watch every pointer move, <span className="shine">one step at a time</span>.
        </h2>
        <p className="hero-text">
          Fourteen classic structures. Run an operation and every step plays out on screen, recorded from the real
          algorithm while it ran: nothing is faked, and every step shows its line of pseudocode, its explanation and
          its cost.
        </p>
        <ul className="hero-facts" aria-label="At a glance">
          <li><strong>{STRUCTURES.length}</strong> structures</li>
          <li><strong>Every</strong> step recorded</li>
          <li><strong>Step</strong>, scrub and replay</li>
          <li><strong>Undo</strong>, export and import</li>
        </ul>
        <div className="hero-actions">
          <button type="button" className="primary big" onClick={() => onChoose('avl')}>Start with an AVL tree</button>
          <button type="button" className="big" onClick={() => onChoose('stack')}>Start simple: a stack</button>
        </div>
      </section>

      {GROUPS.map((group) => (
        <section className="card-section" key={group} aria-label={group}>
          <h3>{group}</h3>
          <ul className="cards">
            {STRUCTURES.filter((structure) => STRUCTURE_INFO[structure].group === group).map((structure) => {
              const info = STRUCTURE_INFO[structure];
              const style = { '--hue': info.hue, '--order': order++ } as CSSProperties;
              return (
                <li key={structure} style={style}>
                  <button type="button" className="card" onClick={() => onChoose(structure)} aria-label={`Open ${info.label}`}>
                    <Preview structure={structure} />
                    <span className="card-title">{info.icon}{info.label}</span>
                    <span className="card-blurb">{info.blurb}</span>
                    <span className="card-costs">
                      {info.costs.map((cost) => <span key={cost} className="chip">{cost}</span>)}
                    </span>
                  </button>
                </li>
              );
            })}
          </ul>
        </section>
      ))}

      <footer className="home-foot">
        <p>
          Tip: on a structure page, press <kbd>Space</kbd> to play or pause, <kbd>←</kbd> <kbd>→</kbd> to step,{' '}
          <kbd>Ctrl</kbd>+<kbd>Z</kbd> to undo, and <kbd>Ctrl</kbd>+<kbd>B</kbd> to show or hide the sidebar.
        </p>
        <p>
          <a href="https://github.com/its-harsh-here/dslab" rel="noreferrer">Source code</a> · MIT license
        </p>
      </footer>
    </div>
  );
}
