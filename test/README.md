# Ruumble Client: local test tools

- `local-stack.sh up|down`: Mumble server (Docker) plus Ruumble service on the host, port 64080.
- `node seed.cjs`: test data on the local server: room descriptions, and everyone may administer the building (to try
  right click menus and moving people). Never against a real server.
- `run-desk.sh <Name> [fresh]`: Ruumble Client headless (offscreen) in an isolated HOME under `.work/`,
  connects as `<Name>`; WebEngine DevTools on 127.0.0.1:9333 (`DEVTOOLS_PORT`). Never touches your real settings.
  `DESK_BRIDGE_URL=http://127.0.0.1:64080` sets a fixed service address instead of discovery.
- `cdp.mjs shot <png>` / `cdp.mjs eval '<js>'` / `cdp.mjs click <x> <y>`: look into and drive the embedded view (raw CDP;
  Playwright's `connectOverCDP` does not work with QtWebEngine). `click` is a real, trusted mouse click: needed for
  light dismiss of popovers and anything else that ignores synthetic events.
- The engine is Chromium 102 (Qt WebEngine 6.4): `cdp.mjs eval "navigator.userAgent"`. The web UI must work there.
- `click.sh '<label prefix>'`: clicks a button by aria-label. Rooms are entered by **double-click**
  (single click selects):

  ```sh
  node cdp.mjs eval "(()=>{const b=[...document.querySelectorAll('[aria-label]')].find(b=>b.getAttribute('aria-label').startsWith(\"Ben's office\"));b.dispatchEvent(new MouseEvent('dblclick',{bubbles:true}));return 'ok'})()"
  ```

Mute/deaf are shown through `aria-pressed`, not the label.
