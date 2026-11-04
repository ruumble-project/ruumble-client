# Engine compatibility for older Ruumble services

Ruumble Desk shows the web UI in Qt WebEngine 6.4 (Chromium 102). The web UI handles that itself since the fix of
2026-11-04 (`web/src/main.ts`, fallbacks in the CSS), but services still running an older image serve a web UI
without it. `RuumbleView` therefore injects, only into engines that need it:

- `popover-polyfill.min.js`: `@oddbird/popover-polyfill` 0.7.3 (`dist/popover.iife.min.js`, BSD-3-Clause, OddBird,
  license in `LICENSE.popover-polyfill`), loaded when `HTMLElement.prototype.showPopover` is missing
- a few CSS fallbacks (board background, board width under `@supports not (width: 1cqw)`, card hover) when
  `color-mix()` is missing. They must win the cascade: the UI's own declarations contain `var()` and are therefore
  accepted when parsed and only dropped when computed, so the fallbacks use a higher specificity (`.board.board.board`)

Update the copy together with the version in `web/package.json`. This can go once every service runs a web UI from
this repo (0.33.0 or newer).
