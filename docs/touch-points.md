# Mumble touch points

Everything Ruumble Desk changes in Mumble's own files, as applied by `patches/mumble/0001-ruumble-desk-touch-points.patch`
onto the tag in `pins.env`. All code changes sit under `#ifdef USE_RUUMBLE`; the rest of Ruumble Desk lives in `client/`
(copied to `src/mumble/ruumble/` in the work tree). This list is the basis for splitting the patch into a stack later
(one patch per touch point) and the first thing to read when a Mumble update brings conflicts.

| File | Where | What | Why |
| --- | --- | --- | --- |
| `src/mumble/CMakeLists.txt` | block at the end | option `ruumble` (on by default): compiles Ruumble's plugin core from `RUUMBLE_SOURCE_DIR/plugin/src` and the `client/` sources into `mumble_client_object_lib`, fetches IXWebSocket, links Qt WebEngine and WebChannel, defines `USE_RUUMBLE` and `RUUMBLE_CORE_VERSION` (read from `plugin/CMakeLists.txt`) | build the core into the client instead of loading the plugin |
| `src/mumble/Log.h`, `Log.cpp` | `Log::log` | signal `messageLogged(type, plain, ownMessage)`, emitted for every log line | the Desk panel of the web UI shows Mumble's log and chat |
| `src/mumble/MainWindow.h` | class `MainWindow` | member `ruumbleDesk` (`RuumbleIntegration *`) | one entry point for all Desk logic |
| `src/mumble/MainWindow.cpp` | `setupGui` | creates `RuumbleIntegration` | |
| | `setupGui` (window title) | `updateWindowTitle()` instead of Mumble's title | the title comes from the page ("Lobby · Ruumble") |
| | `recreateServerHandler` | `attachServerHandler(sh)` on every new server handler | core lifecycle follows connect and disconnect |
| | end of `setupView` | `applyChrome()` | hides menu bar, toolbar and docks, the building fills the window |
| `src/mumble/PluginManager.cpp` | `rescanPlugins` | skips a plugin named "Ruumble" and logs that | no second connection from an installed external plugin |
| `src/mumble/main.cpp` | top | `RPC_NAME` "RuumbleDesk" and `DBUS_NAME` `net.sourceforge.mumble.ruumbledesk`, used by every `SocketRPC` and D-Bus call and registration below | single instance and `mumble://` links never reach the official Mumble |
| | before `MumbleApplication` | `AA_ShareOpenGLContexts`, application name "Ruumble Desk", organization "Ruumble" | Qt WebEngine needs the attribute; own settings, data and plugin dirs (ADR-0024 D5, D6) |
| | after the application | `setDesktopFileName("ruumble-desk")` | own taskbar entry and icon |
| | before the settings load | `RuumbleIntegration::importMumbleSettings()` when no settings file is given | audio settings and shortcuts of an official Mumble taken over once |

Not touched: Mumble's dialogs, tray icon, global shortcuts, audio, the user tree (hidden, but still the source of
Mumble's channel and user menus, see `RuumbleIntegration::showChannelMenu`).

Keep this table in step with the patch: a new touch point gets a row here in the same commit.
