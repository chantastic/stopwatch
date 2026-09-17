import test from "node:test";
import assert from "node:assert/strict";
import { setImmediate as immediate } from "node:timers/promises";
import { fileURLToPath } from "node:url";
import { build } from "esbuild";

// Resolve the pinned browser library's extensionless imports exactly as the
// production bundle does. No private replacement for Transport is under test.
const result = await build({
  entryPoints: [fileURLToPath(new URL("../src/transport.js", import.meta.url))],
  bundle: true, write: false, format: "esm", platform: "browser", target: "chrome120",
});
const { SafeTransport } = await import(`data:text/javascript;base64,${Buffer.from(result.outputFiles[0].contents).toString("base64")}`);

function port({ write = () => {}, cancel = () => {}, close } = {}) {
  const value = {
    writes: [], closed: false,
    readable: new ReadableStream({ cancel }),
    writable: new WritableStream({ write: bytes => { value.writes.push(bytes); return write(bytes); } }),
    async close() {
      assert.equal(this.readable?.locked ?? false, false, "reader released before closing");
      assert.equal(this.writable?.locked ?? false, false, "writer released before closing");
      await close?.(); this.closed = true;
    },
  };
  return value;
}

function transport(device, options = {}) {
  const value = new SafeTransport(device, false, true, { writeTimeoutMs: 40, disconnectTimeoutMs: 60, ...options });
  // Upstream readLoop emits lifecycle trace messages even with tracing disabled.
  value.trace = () => {};
  return value;
}

test("successful writes retain upstream SLIP escaping and release the lock", async () => {
  const device = port(), connection = transport(device);
  await connection.write(Uint8Array.of(1, 0xc0, 0xdb, 2));
  assert.deepEqual(device.writes[0], Uint8Array.of(0xc0, 1, 0xdb, 0xdc, 0xdb, 0xdd, 2, 0xc0));
  assert.equal(device.writable.locked, false);
  assert.equal(connection.activeWriter, null);
  await connection.disconnect(); assert.equal(device.closed, true);
});

test("unplug/rejected write releases its writer so disconnect and retry can finish", async () => {
  const unplug = new DOMException("Device lost", "NetworkError");
  const device = port({ write: () => { throw unplug; } }), connection = transport(device);
  await assert.rejects(connection.write(Uint8Array.of(1)), error => error === unplug);
  assert.equal(device.writable.locked, false);
  assert.equal(connection.activeWriter, null);
  await connection.disconnect(); assert.equal(device.closed, true);
  const next = port(), retry = transport(next);
  await retry.write(Uint8Array.of(2)); await retry.disconnect();
  assert.equal(next.closed, true);
});

test("a stalled write has a deadline and releases the lock", async () => {
  let complete;
  const device = port({ write: () => new Promise(resolve => { complete = resolve; }) });
  const connection = transport(device, { writeTimeoutMs: 20 });
  const started = performance.now();
  try {
    await assert.rejects(connection.write(Uint8Array.of(1)), /USB write timed out/);
    assert.ok(performance.now() - started < 1000);
    assert.equal(device.writable.locked, false);
  } finally { complete(); }
  await connection.disconnect();
});

test("a disconnected port fails explicitly instead of silently dropping writes", async () => {
  const device = port(); device.writable = null;
  await assert.rejects(transport(device).write(Uint8Array.of(1)), /USB disconnected/);
});

test("disconnect cancels the actual upstream reader and waits for release", async () => {
  let cancelled = false;
  const device = port({ cancel: () => { cancelled = true; } }), connection = transport(device);
  const reading = connection.readLoop();
  await immediate(); assert.equal(device.readable.locked, true);
  await connection.disconnect(); await reading;
  assert.equal(cancelled, true); assert.equal(device.closed, true);
  assert.equal(device.readable.locked, false);
});

test("an externally locked stream cannot make disconnect wait forever", async () => {
  const device = port(), externalWriter = device.writable.getWriter();
  const connection = transport(device, { disconnectTimeoutMs: 20 });
  try {
    await assert.rejects(connection.disconnect(), /USB cleanup timed out/);
    assert.equal(device.closed, false);
  } finally { externalWriter.releaseLock(); }
  await connection.disconnect(); assert.equal(device.closed, true);
});

test("reader cancellation that never resolves has a bounded failure", async () => {
  let complete;
  const device = port({ cancel: () => new Promise(resolve => { complete = resolve; }) });
  const connection = transport(device, { disconnectTimeoutMs: 20 });
  const reading = connection.readLoop(); await immediate();
  try { await assert.rejects(connection.disconnect(), /USB cleanup timed out/); }
  finally { complete(); await reading; }
  assert.equal(device.readable.locked, false);
});

test("port close rejection is returned with both stream locks released", async () => {
  const unplug = new DOMException("Device lost", "NetworkError");
  const device = port({ close: () => { throw unplug; } }), connection = transport(device);
  await assert.rejects(connection.disconnect(), error => error === unplug);
  assert.equal(device.readable.locked, false); assert.equal(device.writable.locked, false);
});

test("port close that never resolves cannot strand installer cleanup", async () => {
  let complete;
  const device = port({ close: () => new Promise(resolve => { complete = resolve; }) });
  const connection = transport(device, { disconnectTimeoutMs: 20 });
  try { await assert.rejects(connection.disconnect(), /USB cleanup timed out/); }
  finally { complete(); }
});
