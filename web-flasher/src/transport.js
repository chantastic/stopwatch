import { Transport } from "esptool-js";

const pause = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));

async function bounded(operation, milliseconds, message) {
  let timer;
  try {
    return await Promise.race([
      operation,
      new Promise((_, reject) => { timer = setTimeout(() => reject(new Error(message)), milliseconds); }),
    ]);
  } finally { clearTimeout(timer); }
}

// Keep esptool-js 0.6.1's framing, reset, and reader implementation. Its write()
// leaks a writer lock on rejection, and disconnect() waits for locks forever.
// Only adapt those two boundaries so unplugging cannot strand the installer UI.
export class SafeTransport extends Transport {
  constructor(device, tracing = false, enableSlipReader = true, options = {}) {
    super(device, tracing, enableSlipReader);
    this.writeTimeoutMs = options.writeTimeoutMs ?? 10000;
    this.disconnectTimeoutMs = options.disconnectTimeoutMs ?? 2500;
    this.activeWriter = null;
  }

  async write(data) {
    if (!this.device.writable) throw new Error("USB disconnected. Reconnect the badge and retry.");
    const bytes = this.slipWriter(data);
    const writer = this.device.writable.getWriter();
    this.activeWriter = writer;
    try {
      if (this.tracing) this.trace(`Write ${bytes.length} bytes: ${this.hexConvert(bytes)}`);
      await bounded(writer.write(bytes), this.writeTimeoutMs, "USB write timed out. Reconnect the badge and retry.");
    } finally {
      // releaseLock is also allowed while a write is pending. On a timed-out
      // operation, cleanup can then attempt closing the port with its own bound.
      try { writer.releaseLock(); } finally {
        if (this.activeWriter === writer) this.activeWriter = null;
      }
    }
  }

  async disconnect() {
    const deadline = performance.now() + this.disconnectTimeoutMs;
    const message = "USB cleanup timed out. Unplug the badge and reload before retrying.";
    const remaining = () => Math.max(1, deadline - performance.now());
    const finish = operation => bounded(operation, remaining(), message);
    let failure;

    if (this.device.readable?.locked && this.reader) {
      try { await finish(this.reader.cancel()); } catch (error) { failure = error; }
    }
    if (this.activeWriter && performance.now() < deadline) {
      try { await finish(this.activeWriter.abort()); } catch (error) { failure ??= error; }
    }

    // The upstream read loop releases its lock in finally after cancellation.
    // Do not call its unbounded waitForUnlock() or close a still-locked port.
    while (this.device.readable?.locked || this.device.writable?.locked) {
      if (performance.now() >= deadline) throw new Error(message);
      await pause(Math.min(20, remaining()));
    }
    if (performance.now() >= deadline) throw new Error(message);
    try { await finish(this.device.close()); } finally { this.reader = undefined; }
    if (failure) throw failure;
  }
}
