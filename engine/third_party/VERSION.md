# Vendored third-party code

Only these two libraries may live here (CLAUDE.md, decision D6 in
docs/operation-contract.md). Do not edit them; replace them with a newer
pinned release instead, and update this file.

| Library | Version | Source | SHA-256 of the header | Licence | Used by |
| --- | --- | --- | --- | --- | --- |
| nlohmann/json | v3.12.0 (tag commit `55f93686c015`) | release asset `json.hpp` from https://github.com/nlohmann/json/releases/tag/v3.12.0 (identical to `single_include/nlohmann/json.hpp` at the tag) | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` | MIT (`nlohmann/LICENSE.MIT`) | `engine/src/json/` and native tests only |
| doctest | v2.5.3 (tag commit `2d0a9359a60c`) | `doctest/doctest.h` at https://github.com/doctest/doctest/tree/v2.5.3 | `cfd518a3ef90f67e1f3ba514df23fb3627437de1a2feeba78cf5062a40021421` | MIT (`doctest/LICENSE.txt`) | native tests only |

Fetched on 2026-10-22. Line endings are LF.
