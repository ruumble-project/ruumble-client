# Ruumble Client: local test tools

- `local-stack.sh up|down`: Mumble server (Docker) plus Ruumble service on the host, port 64080.
- `node seed.cjs`: test data on the local server: room descriptions, and everyone may administer the building (to try
  right click menus and moving people). Never against a real server.
- `run-desk.sh <Name> [fresh]`: Ruumble Client headless (offscreen) in an isolated HOME under `.work/`,
  connects as `<Name>`; WebEngine DevTools on 127.0.0.1:9333 (`DEVTOOLS_PORT`). Never touches your real settings.
  `DESK_BRIDGE_URL=http://127.0.0.1:64080` sets a fixed service address instead of discovery.
  City Map (ticket #8): `DESK_LINK=0` starts without a `mumble://` link, so the City Map opens (or "On your next visit"
  enters a building); `DESK_AUTOCONNECT=1` and `DESK_LAST_SERVER=<name>` preset Mumble's auto connect and last
  building (headquarters: `city-map.json` in the test home's data dir); `DESK_LANG=de_DE` for the German texts.
  `DESK_AUDIO=1` hands over this desktop's PipeWire and PulseAudio (otherwise the client has no sound).
- `migration.sh`: ticket #15, taking over an installed Mumble. Builds fake Mumble setups in isolated homes, starts
  Ruumble Client with `RUUMBLE_MUMBLE_MIGRATION=take|not-now|never` (answers the dialog) and checks the files and,
  through Ice (`migration-ice.cjs`, test server only), that the connection is the registered user with the token.
  Needs `local-stack.sh up`.
- `node transmit.mjs`: ticket #9, how the user transmits through the `desk` object (mode, press, latch, key capture and
  binding, double press, letting go on reload). Needs a client started with `DESK_AUDIO=1 test/run-desk.sh <Name>`,
  which hands over this desktop's PipeWire and PulseAudio so the own talk state is real (the test user transmits the
  real microphone to the local test server while it presses).
- `CDP_WIDTH=900 CDP_HEIGHT=700 node cdp.mjs ...`: the page at another size (default 1280×800), e.g. to check a narrow window.
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
