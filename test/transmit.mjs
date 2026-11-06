// Ticket #9: how the user transmits, through the desk object of a running headless client.
//   DESK_AUDIO=1 test/run-desk.sh <Name> fresh   (needs local-stack.sh up; the audio makes the own talk state real)
//   node test/transmit.mjs
// Switches the mode, presses, latches, binds a key, then reloads the page and checks that the client let go.
// The test opens a second QWebChannel on the page (the page's own one stops working until the reload at the end).
import { execFileSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const cdp = join(dirname(fileURLToPath(import.meta.url)), "cdp.mjs");
const evaluate = (js) => JSON.parse(execFileSync("node", [cdp, "eval", js], { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] }).split("\n").filter((l) => !l.startsWith("page:")).join("\n"));

const prelude = `
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const desk = await new Promise((r) => new QWebChannel(qt.webChannelTransport, (ch) => r(ch.objects.desk)));
  const call = (fn, ...args) => new Promise((r) => desk[fn](...args, r));
  if (typeof desk.transmitMode !== "string") return { error: "the client has no transmitMode" };`;

const run = evaluate(`(async () => {${prelude}
  const r = {};
  r.vad = (await call("setTransmitMode", "vad"), await wait(300), desk.transmitMode);
  r.ptt = (await call("setTransmitMode", "ptt"), await wait(300), desk.transmitMode);
  r.idle = desk.transmitting;
  desk.pushToTalk(true); await wait(500); r.pressed = desk.transmitting;
  desk.pushToTalk(false); await wait(500); r.released = desk.transmitting;
  desk.setLatched(true); await wait(500); r.latched = desk.latched && desk.transmitting;
  const captured = new Promise((res) => desk.keyCaptured.connect(res));
  r.capturing = await call("captureKey");
  desk.capturePageKey(["ControlRight"]);
  r.captured = await captured;
  r.accepted = await call("acceptKey"); await wait(300);
  r.key = desk.pttKey;
  r.doublePush = (await call("setPttOption", "doublePush", true), await wait(300), desk.pttOptions.doublePush);
  await call("setPttOption", "doublePush", false);
  setTimeout(() => location.reload(), 200); // after the answer is out
  return r;
})()`);

// the page reloads now: try until the new one answers
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let after = null;
for (let i = 0; i < 20 && !after; i++) {
  await sleep(1000);
  try {
    after = evaluate(`(async () => {
  await new Promise((r) => (document.readyState === "complete" ? r() : addEventListener("load", r)));
  await new Promise((r) => setTimeout(r, 1500));${prelude}
  const r = { latched: desk.latched, transmitting: desk.transmitting, mode: desk.transmitMode };
  await call("setTransmitMode", "ptt");
  return r;
})()`);
  } catch {
    // still loading
  }
}
after ??= {};

const checks = [
  ["switches to voice activation", run.vad === "vad"],
  ["switches to push-to-talk", run.ptt === "ptt"],
  ["silent without a press", run.idle === false],
  ["transmits while pressed", run.pressed === true],
  ["stops when released", run.released === false],
  ["a latch transmits", run.latched === true],
  ["captures a key from the page", run.capturing === true && run.captured?.codes?.[0] === "ControlRight" && run.captured?.warn === false],
  ["binds it as Mumble's push-to-talk key", run.accepted === true && run.key?.set === true && run.key?.codes?.[0] === "ControlRight"],
  ["sets Mumble's double press", run.doublePush === true],
  ["a reload lets go of the latch", after.latched === false && after.transmitting === false && after.mode === "ptt"],
];
let failed = 0;
for (const [name, ok] of checks) {
  console.log(`${ok ? "ok  " : "FAIL"} ${name}`);
  if (!ok) failed++;
}
if (failed) {
  console.log(JSON.stringify({ run, after }, null, 2));
  process.exit(1);
}
