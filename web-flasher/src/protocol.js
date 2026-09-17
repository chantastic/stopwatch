// This is the same bounded, nonce-bound clock/readiness contract as
// scripts/provision-clock.py. Never expose unrelated serial output in the UI.
export const MIN_EPOCH = 1704067200;
export const MAX_EPOCH = 4102444800;
export const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

async function bounded(promise, milliseconds, message) {
  let timer;
  try {
    return await Promise.race([promise, new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(message)), milliseconds);
    })]);
  } finally { clearTimeout(timer); }
}

export function nonce() {
  return Array.from(crypto.getRandomValues(new Uint8Array(12)), b => b.toString(16).padStart(2, "0")).join("");
}

export function validateClock(reply, now, offset) {
  if (reply.protocol !== 1 || reply.ok !== true || reply.valid !== true ||
      !["computer", "rtc"].includes(reply.source)) throw new Error("The badge did not verify its clock. Retry clock setup.");
  if (reply.offset_minutes !== offset) throw new Error("The badge's time zone did not match this computer.");
  for (const key of ["epoch", "rtc_epoch"]) {
    if (!Number.isInteger(reply[key]) || reply[key] < MIN_EPOCH || reply[key] >= MAX_EPOCH || Math.abs(reply[key] - now) > 3)
      throw new Error("Clock readback did not match this computer's time.");
  }
  if (Math.abs(reply.epoch - reply.rtc_epoch) > 1) throw new Error("The badge's two clocks disagree.");
}

export function validateReadiness(reply, build, requireStore = true) {
  if (reply.build !== build) throw new Error("The running firmware does not match this release.");
  if (reply.board !== 30 || reply.flash_bytes !== 16777216 || reply.psram_bytes !== 8388608)
    throw new Error("This release requires an M5Stack StopWatch with 16 MB flash and 8 MB PSRAM.");
  if (typeof reply.store_ready !== "boolean" || (requireStore && reply.store_ready !== true))
    throw new Error("Profile storage needs preparation. Existing data has not been erased.");
  if (reply.clock_valid !== true || reply.rtc !== true) throw new Error("The hardware clock is not ready.");
  if (reply.setup !== false || reply.wifi_mode !== 0 || reply.bluetooth !== 0)
    throw new Error("Close badge setup so the installer can verify normal offline operation.");
}

export function safeDiagnostics(reply, release) {
  // Do not include profile fields, credentials, USB identifiers, or raw logs.
  const result = { release };
  for (const key of ["build", "framework", "board", "flash_bytes", "psram_bytes", "store_ready", "clock_valid", "rtc", "setup", "wifi_mode", "bluetooth"])
    if (["string", "number", "boolean"].includes(typeof reply[key])) result[key] = reply[key];
  return result;
}

export class ClockConnection {
  constructor(port) { this.port = port; this.pending = ""; this.lines = []; this.stopped = false; this.failure = null; }
  async open() {
    try {
      await bounded(this.port.open({ baudRate: 115200 }), 3000, "USB connection timed out.");
      this.reader = this.port.readable.getReader();
      this.writer = this.port.writable.getWriter();
      this.pump = this.read();
    } catch (error) { await this.close(); throw error; }
  }
  async read() {
    const decoder = new TextDecoder();
    let discarding = false;
    try {
      while (!this.stopped) {
        const { value, done } = await this.reader.read();
        if (done) break;
        for (const char of decoder.decode(value, { stream: true })) {
          if (char === "\n") {
            if (!discarding && /^(CLOCK_ACK|CLOCK_STATUS|CONFERENCE_STATUS|STORAGE_ACK) /.test(this.pending)) {
              this.lines.push(this.pending.trim());
              if (this.lines.length > 24) this.lines.shift();
            }
            this.pending = ""; discarding = false;
          } else if (!discarding) {
            this.pending += char;
            if (this.pending.length > 2048) { this.pending = ""; discarding = true; }
          }
        }
      }
    } catch { if (!this.stopped) this.failure = new Error("USB disconnected. Reconnect the same badge and retry clock setup."); }
    finally { if (!this.stopped && !this.failure) this.failure = new Error("USB connection closed before verification."); }
  }
  async request(op, prefix, fields = {}, timeout = 3000) {
    const requestNonce = nonce();
    const deadline = performance.now() + timeout;
    try {
      await bounded(this.writer.write(new TextEncoder().encode(JSON.stringify({ ...fields, op, nonce: requestNonce }) + "\n")), timeout, "USB write timed out. Reconnect the badge and retry.");
    } catch (error) { this.failure = error; throw error; }
    while (performance.now() < deadline) {
      if (this.failure) throw this.failure;
      while (this.lines.length) {
        const line = this.lines.shift();
        if (!line.startsWith(prefix + " ")) continue;
        try {
          const response = JSON.parse(line.slice(prefix.length + 1));
          if (response.nonce === requestNonce) return response;
        } catch { /* Ignore malformed or unrelated data. */ }
      }
      await delay(25);
    }
    throw new Error("The badge did not reply in time. Close other USB tools and retry.");
  }
  async provision(build) {
    let ack, offset;
    const deadline = performance.now() + 30000;
    while (performance.now() < deadline) {
      const now = Math.floor(Date.now() / 1000);
      if (now < MIN_EPOCH || now >= MAX_EPOCH) throw new Error("Check this computer's date and time first.");
      offset = -new Date().getTimezoneOffset();
      try { ack = await this.request("clock_set", "CLOCK_ACK", { epoch: now, offset_minutes: offset }, 2000); }
      catch (error) { if (this.failure || performance.now() >= deadline) throw error; continue; }
      validateClock(ack, Math.floor(Date.now() / 1000), offset);
      break;
    }
    if (!ack) throw new Error("Clock setup could not be verified.");
    await delay(1200);
    const status = await this.request("clock_status", "CLOCK_STATUS");
    validateClock(status, Math.floor(Date.now() / 1000), offset);
    if (status.rtc_epoch <= ack.rtc_epoch) throw new Error("The hardware clock is not advancing.");
    const ready = await this.request("status", "CONFERENCE_STATUS");
    validateReadiness(ready, build);
    return ready;
  }
  async close() {
    this.stopped = true;
    if (this.reader) {
      try { await bounded(this.reader.cancel(), 1500, "USB reader did not stop."); } catch {}
      try { this.reader.releaseLock(); } catch {}
      try { await bounded(this.pump, 1500, "USB reader did not finish."); } catch {}
    }
    if (this.writer) { try { this.writer.releaseLock(); } catch {} }
    try { await bounded(this.port.close(), 2000, "USB connection did not close."); } catch {}
  }
}
