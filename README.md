# DSLab: Interactive Data Structure Simulator

A college data-structures project. A C++17 engine implements every structure and
algorithm; it is compiled to WebAssembly and drawn in the browser with React and SVG.
Every step you see is a frame recorded by the C++ code while it runs. There is no
JavaScript copy of any algorithm and no backend.

**Live site:** https://&lt;user&gt;.github.io/dslab/ (deployed by GitHub Actions from `main`;
see [Deployment](#deployment-github-pages)).

Seven structures: stack, queue, singly linked list, ordinary binary tree, binary search
tree, AVL tree and red-black tree. For each operation you can play, pause, step and scrub
through every frame with its pseudocode line, the selected node's properties and the
operation counters beside it; pan and zoom; run batch, seeded random and preset examples
(AVL rotation cases, red-black fix-up cases); undo and redo; and export a structure to a
JSON file and import it again, checked in C++ first.

![The stack after push 10, push 20, pop](docs/screenshots/stack-after-push10-push20-pop.png)

## Documentation

| Document | Contents |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Components, which files hold C++ algorithms and which hold browser presentation, the path from a click to C++ and back to SVG, builds, CI |
| [docs/operation-contract.md](docs/operation-contract.md) | Every operation, error code, the JSON request/response and frame schemas, limits, decisions |
| [docs/complexity.md](docs/complexity.md) | Time complexity of every operation, with tracing and rendering cost listed separately and measured |
| [docs/memory.md](docs/memory.md) | Node ownership and memory management |
| [docs/trace-playback.md](docs/trace-playback.md) | Frame format, playback, undo/redo, export/import |
| [docs/avl.md](docs/avl.md), [docs/red-black.md](docs/red-black.md) | Design and pseudocode of the two balanced trees |
| [docs/demo-script.md](docs/demo-script.md) | A five-minute demo |
| [docs/viva-questions.md](docs/viva-questions.md) | 30 viva questions with model answers |
| [docs/plan.md](docs/plan.md), [docs/toolchain.md](docs/toolchain.md) | Milestones; pinned tool versions |

## Setup

Tested on Ubuntu 24.04 under WSL2 on ARM64 (aarch64), and on GitHub's `ubuntu-latest`
(x64) in CI. Any Linux or macOS machine with the same tools should work.

| Tool | Version | Why |
| --- | --- | --- |
| g++ (or clang++) | C++17 support; g++ 13.3 tested | native build and tests |
| CMake | 3.20 or newer; 3.28 tested | builds both native and WebAssembly |
| Ninja | 1.11 tested | CMake generator |
| Python 3, git | any recent | needed by emsdk |
| Node.js | 22.12 or newer; 24 tested (via nvm) | Vite, React, TypeScript, Playwright |
| Emscripten (emsdk) | **exactly the version in [`.emsdk-version`](.emsdk-version)** (6.0.13) | compiles C++ to WebAssembly |

On Ubuntu, `scripts/setup-system.sh` installs the apt packages, nvm with Node 24 and
Playwright's Chromium. Run it as yourself; it asks for `sudo` only where needed:

```bash
bash scripts/setup-system.sh
```

Install emsdk once (any folder works; `~/tools/emsdk` is the one used here):

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/tools/emsdk
```

Get the code:

```bash
git clone https://github.com/<user>/dslab.git
```

```bash
cd dslab
```

From the repository root, install and activate the pinned Emscripten version. Both
commands read it from `.emsdk-version`, the same file the build script and CI read:

```bash
~/tools/emsdk/emsdk install "$(cat .emsdk-version)"
```

```bash
~/tools/emsdk/emsdk activate "$(cat .emsdk-version)"
```

Tell the build where emsdk is, in every new shell or in `~/.bashrc`:

```bash
export EMSDK=$HOME/tools/emsdk
```

Install the web dependencies at the exact versions in `package-lock.json`:

```bash
npm ci
```

Install Playwright's bundled Chromium for the browser tests (not Google Chrome, which
does not exist for Linux ARM64):

```bash
npx playwright install --no-shell chromium
```

To check the whole toolchain at once:

```bash
bash scripts/check-env.sh
```

`npm run build:wasm` uses `$EMSDK` if it is set, otherwise an `em++` already on your
PATH (for example after `source ~/tools/emsdk/emsdk_env.sh`). If it finds neither, it
stops and explains what to set. It refuses any Emscripten version other than the one in
`.emsdk-version`.

## Build and run

| Command | What it does | Output |
| --- | --- | --- |
| `npm run build:wasm` | Compiles the engine with Emscripten and copies it next to the web sources | `build/wasm/`, `web/src/generated/` |
| `npm run dev` | `build:wasm`, then the Vite dev server at http://localhost:5173/dslab/ | — |
| `npm run build` | `build:wasm`, a TypeScript check, then the production build | `dist/` |
| `npm run preview` | Serves `dist/` at http://localhost:4173/dslab/, exactly as GitHub Pages will | — |

```bash
npm run dev
```

Then open http://localhost:5173/dslab/. The site lives under `/dslab/` everywhere
(`base` in `vite.config.ts`), so local runs use the same URLs as the deployed site.

## Tests

| Command | What it runs | Time here |
| --- | --- | --- |
| `npm run test:cpp` | Native CMake build and the C++ tests (doctest): every operation, an independent invariant checker after every operation, seeded random sequences against standard-library oracles, golden JSON files, undo/redo and import | about 35 s |
| `npm run test:cpp:asan` | The same tests under AddressSanitizer + UBSan, built with `-O1` | about 25 min |
| `npm run test:e2e` | Production build, the native command runner, then the whole Playwright suite (`web/e2e/`) against `npm run preview` in bundled Chromium: every step on screen matches its C++ frame, playback, examples, undo/redo, import, and native-versus-Wasm parity on 1,298 requests | about 2.5 min |
| `npm run test:e2e:smoke` | Only the tests tagged `@smoke` (what CI runs before deploying), against the existing `dist/`: run `npm run build` first | about 20 s |

Run them in this order on a fresh clone:

```bash
npm run test:cpp
```

```bash
npm run test:cpp:asan
```

```bash
npm run test:e2e
```

## Using DSLab

1. Choose a structure on the left.
2. Type a value and press the operation button (**Push**, **Enqueue**, **Insert at
   index**, **Insert**…), or open **Examples** for a batch (`10, 20, 30`), a seeded random
   structure or a preset (AVL LL/RR/LR/RL, the seven red-black fix-up cases, a BST delete
   with a deeper successor, sorted BST inserts).
3. Watch the operation play. The right-hand panels show the explanation, the pseudocode
   line of the current step, the selected node (click a node; selection is by node ID)
   and the cost: algorithm counters and Big-O, with the tracing cost apart.
4. Control playback with the buttons under the drawing or the keyboard (focus outside
   text boxes): `Space` play/pause, `←`/`→` step, `Home` first step, `End` jump to result,
   `R` replay, `[`/`]` speed, `+`/`-`/`0` zoom and fit, `Shift`+arrows pan,
   `Ctrl+Z`/`Ctrl+Y` undo/redo. Drag the drawing to pan; use the wheel to zoom.
5. **Export** saves the structure as a JSON file; **Import** loads one (validated in C++;
   an invalid file changes nothing). **Reset** clears the structure.

New operations wait until the one on screen shows its result (**Jump to result** skips
ahead).

## Deployment (GitHub Pages)

`.github/workflows/deploy.yml` runs on every push to `main` and from the Actions tab
(**Run workflow**). On `ubuntu-latest` it installs the Emscripten version in
`.emsdk-version` and Node 24, runs `npm ci`, `npm run test:cpp` and `npm run build`,
installs Playwright's Chromium, runs `npm run test:e2e:smoke` against `npm run preview`,
and, only if all of that passed, publishes `dist/` with the official GitHub Pages actions
(`configure-pages`, `upload-pages-artifact`, `deploy-pages`).

GitHub settings to change, once:

1. **Create the repository as public and name it exactly `dslab`.** The name is the URL
   path: `https://<user>.github.io/dslab/` must match `base: '/dslab/'` in
   `vite.config.ts` (also used by `playwright.config.ts`). With a free GitHub account,
   Pages works only for public repositories.
2. Push `main` to it.
3. **Settings → Pages → Build and deployment → Source: GitHub Actions.** (Not "Deploy
   from a branch"; there is no `gh-pages` branch.)
4. **Settings → Actions → General → Actions permissions:** allow actions created by
   GitHub (the default, "Allow all actions and reusable workflows", is fine). The
   workflow asks for its own token permissions (`contents: read`, `pages: write`,
   `id-token: write`), so "Workflow permissions" can stay at the default read-only.
5. **Settings → Environments → github-pages** is created automatically on the first
   deployment and lets only `main` deploy. Leave it as it is.
6. Open the **Actions** tab, choose **Test and deploy** and press **Run workflow** (or
   push to `main`). When it finishes, the `deploy` job shows the site's URL.

To use a different repository name, change `base` in `vite.config.ts` and `baseURL` and
the server `url` in `playwright.config.ts` to `/<name>/`.

## Known limitations

- **Size and values:** at most 200 nodes per structure, values are 32-bit signed
  integers, node IDs are never reused. Large trees get small on screen; use zoom and pan.
- **Duplicates** are rejected in the BST, AVL and red-black trees (allowed in the linear
  structures and the ordinary binary tree).
- **Nothing is saved between visits.** All state lives in the page; reloading starts
  empty. Use Export to keep a structure.
- **Undo history** is 50 steps per structure (undo and redo together) and only covers
  operations, not playback position or the page's operation log.
- **Large responses:** a traversal of a 200-node tree records 402 frames, about 6 MB of
  JSON. It works, but click-to-first-frame time on slow devices has not been measured.
- **One operation at a time:** new operations wait until the one on screen reaches its
  result; batches run one request after another.
- **The ordinary binary tree** has no batch insert: positions are chosen by clicking a
  parent, so random examples pick a random free child slot.
- **Browsers:** built for Chrome/Edge 111+, Firefox 114+ and Safari 16.4+, but the
  automated tests run only in Playwright's Chromium.
- **CI** runs the native tests and the browser smoke tests on every push; the sanitizer
  suite (about 25 minutes) and the full browser suite are run locally.
- **Unexpected C++ exceptions** (none are known) are reported as `INTERNAL_ERROR`, but the
  engine does not yet roll the structure back to its state before the operation.
- **No formal accessibility audit** has been done, although every control has a label,
  the explanation is announced, keyboard control is complete and reduced motion is
  respected.
- Explanations are in English only.

## If something goes wrong

- **`build:wasm failed: Emscripten was not found`**: set `EMSDK` (see Setup).
- **`em++ is version X, but .emsdk-version pins Y`**: from the repository root run
  `~/tools/emsdk/emsdk install "$(cat .emsdk-version)"` and
  `~/tools/emsdk/emsdk activate "$(cat .emsdk-version)"`.
- **The page says "The C++ engine could not be loaded"**: it names the file, the exact
  URL it tried and what went wrong (for example HTTP 404). In development run
  `npm run build:wasm`; for a deployment rebuild and upload all of `dist/`.
