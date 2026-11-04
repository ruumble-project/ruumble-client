// Minimal CDP client for Ruumble Desk's embedded view.
//   node cdp.mjs shot <file.png>       screenshot
//   node cdp.mjs eval '<js expression>' evaluate in the page (awaits promises), prints JSON
import { writeFileSync } from "node:fs";
const port = process.env.DEVTOOLS_PORT || 9333;
const targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
const target = targets.find((t) => t.type === "page" && t.url.startsWith("http://127.0.0.1:64080")) || targets.find((t) => t.type === "page");
if (!target) { console.error("no page", targets); process.exit(1); }
const ws = new WebSocket(target.webSocketDebuggerUrl);
let id = 0; const waiting = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waiting.has(m.id)) { waiting.get(m.id)(m); waiting.delete(m.id); } };
await new Promise((r) => (ws.onopen = r));
const send = (method, params = {}) => new Promise((r) => { const i = ++id; waiting.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const [cmd, arg] = process.argv.slice(2);
await send("Emulation.setDeviceMetricsOverride", { width: 1280, height: 800, deviceScaleFactor: 1, mobile: false });
console.error("page:", target.url);
if (cmd === "shot") {
  const r = await send("Page.captureScreenshot", { format: "png" });
  writeFileSync(arg, Buffer.from(r.result.data, "base64"));
  console.log("saved", arg);
} else {
  const r = await send("Runtime.evaluate", { expression: arg, awaitPromise: true, returnByValue: true });
  console.log(JSON.stringify(r.result.exceptionDetails ?? r.result.result.value, null, 1));
}
ws.close();
