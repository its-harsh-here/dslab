// The overview page: a short introduction and one card per structure.
import { STRUCTURES, type StructureName } from '../engine/protocol';
import { GROUPS, STRUCTURE_INFO } from './structures';

export function Home({ onChoose }: { onChoose: (structure: StructureName) => void }) {
  return (
    <div className="home">
      <section className="hero" aria-labelledby="hero-title">
        <h2 id="hero-title">Data structure visualizer</h2>
        <p className="hero-text">
          Choose a structure, run an operation, and step through it. Each step shows what changed, the line of
          pseudocode being executed, and how much work has been done so far.
        </p>
      </section>

      {GROUPS.map((group) => (
        <section className="card-section" key={group} aria-labelledby={`group-${group.replace(/\W+/g, '-')}`}>
          <h3 id={`group-${group.replace(/\W+/g, '-')}`}>{group}</h3>
          <ul className="cards">
            {STRUCTURES.filter((structure) => STRUCTURE_INFO[structure].group === group).map((structure) => {
              const info = STRUCTURE_INFO[structure];
              return (
                <li key={structure}>
                  <button type="button" className="card" onClick={() => onChoose(structure)}>
                    <span className="card-icon">{info.icon}</span>
                    <span className="card-body">
                      <span className="card-title">{info.label}</span>
                      <span className="card-blurb">{info.blurb}</span>
                      <span className="card-costs">{info.costs.join(' · ')}</span>
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
          Keyboard: <kbd>Space</kbd> play or pause · <kbd>←</kbd> <kbd>→</kbd> step · <kbd>Ctrl</kbd> <kbd>Z</kbd> undo ·{' '}
          <kbd>Ctrl</kbd> <kbd>B</kbd> show or hide the sidebar
        </p>
        <p>
          <a href="https://github.com/its-harsh-here/dslab" rel="noreferrer">Source code</a> · MIT license
        </p>
      </footer>
    </div>
  );
}
