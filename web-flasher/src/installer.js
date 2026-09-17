import { ESPLoader } from "esptool-js";
import { SafeTransport } from "./transport.js";
import { md5 } from "js-md5";
import { ClockConnection, delay, safeDiagnostics } from "./protocol.js";
import { sha256, verifiedRead, verifySecurity, verifyBootSelector, validateArtifact, validateManifest } from "./guards.js";

const RELEASE = "/stopwatch/install/releases/conference-factory-3/";
const $ = id => document.getElementById(id);
let busy = false, selectedPort, manifest, diagnostics;

function status(message, progress = 0) {
  $("status").textContent = message;
  $("progress").value = progress;
}
function controls(active) {
  busy = active;
  if (active) {
    diagnostics = undefined;
    $("copy").disabled = true; $("copy").textContent = "Copy build details";
    $("details").textContent = ""; $("details").hidden = true;
  }
  $("install").disabled = active || !$("hardware").checked;
  $("clock").disabled = active;
  $("hardware").disabled = active;
  $("activity").setAttribute("aria-busy", String(active));
}
async function getRelease() {
  const response = await fetch(RELEASE + "release.json", { credentials: "omit", cache: "no-cache" });
  if (!response.ok) throw new Error("The release is unavailable. Please try again later.");
  manifest = validateManifest(await response.json());
  return manifest;
}
async function components(release) {
  return Promise.all(release.artifacts.map(async artifact => {
    const response = await fetch(RELEASE + artifact.path, { credentials: "omit" });
    if (!response.ok) throw new Error("A firmware component could not be downloaded.");
    const bytes = new Uint8Array(await response.arrayBuffer());
    await validateArtifact(artifact, bytes);
    return { data: bytes, address: artifact.offset };
  }));
}
async function syncClock() {
  const deadline = performance.now() + 15000;
  let connection;
  while (performance.now() < deadline) {
    connection = new ClockConnection(selectedPort);
    try { await connection.open(); break; }
    catch { await delay(400); connection = undefined; }
  }
  if (!connection) throw new Error("The badge is restarting. Keep it plugged in, then choose Set clock & check badge.");
  try {
    const ready = await connection.provision("conference-factory-3");
    diagnostics = safeDiagnostics(ready, "conference-factory-3");
    $("copy").disabled = false;
    status("Ready. Firmware, storage, offline mode, and the advancing clock are verified.", 100);
    $("activity").dataset.result = "success";
  } finally { await connection.close(); }
}

async function install() {
  if (busy || !$("hardware").checked) return;
  let transport, loader, wrote = false, reset = false;
  controls(true); $("activity").dataset.result = "working";
  try {
    // The chooser must stay directly inside the user's click gesture.
    selectedPort = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a, usbProductId: 0x1001 }] });
    status("Downloading and verifying the release…", 5);
    const release = await getRelease();
    const files = await components(release);
    status("Connecting to your StopWatch…", 10);
    transport = new SafeTransport(selectedPort, false);
    // Discard library logs: its normal connection log includes a device MAC.
    loader = new ESPLoader({ transport, baudrate: 115200, terminal: { clean() {}, write() {}, writeLine() {} }, debugLogging: false });
    await loader.detectChip("default_reset");
    if (loader.chip.CHIP_NAME !== "ESP32-S3") throw new Error("This release is for M5Stack StopWatch only. Nothing was written.");
    verifySecurity(await loader.checkCommand("read security info", 0x14, new Uint8Array(), 0, 20, 3000));
    if (loader.chip.postConnect) await loader.chip.postConnect(loader);
    await loader.runStub();
    if (!loader.IS_STUB || await loader.detectFlashSize() !== "16MB") throw new Error("Could not verify the StopWatch's 16 MB flash. Nothing was written.");
    status("Checking the existing storage layout…", 15);
    const partitions = await verifiedRead(loader, transport, 0x8000, 4096);
    if (await sha256(partitions) !== release.partition_table.sector_sha256)
      throw new Error("This device has factory firmware or a different storage layout. This preview installer supports badges already prepared for the conference. Nothing was written. See First install below.");
    verifyBootSelector(await verifiedRead(loader, transport, 0xe000, 8192));
    status("Installing the badge. Keep USB connected…", 20);
    wrote = true;
    await loader.writeFlash({
      fileArray: files, eraseAll: false, compress: true,
      flashSize: "keep", flashMode: "keep", flashFreq: "keep",
      calculateMD5Hash: bytes => md5(bytes),
      reportProgress(index, written, total) { status("Installing the badge. Keep USB connected…", 20 + (index + written / total) / files.length * 65); }
    });
    status("Firmware verified. Restarting and setting your local time…", 90);
    await transport.setRTS(true); await delay(100); await loader.after("hard_reset");
    reset = true;
    await transport.disconnect(); transport = undefined;
    await delay(1200);
    await syncClock();
  } catch (error) {
    $("activity").dataset.result = "error";
    if (error.name === "NotFoundError") status("No device selected. Connect your badge whenever you are ready.");
    else status((wrote ? "Installation needs attention. " : "Installation stopped. ") + error.message);
  } finally {
    if (transport) {
      if (loader && !reset) { try { await transport.setRTS(true); await delay(100); await loader.after("hard_reset"); } catch {} }
      try { await transport.disconnect(); } catch {}
    }
    controls(false);
  }
}

$("install").addEventListener("click", install);
$("clock").addEventListener("click", async () => {
  if (busy) return;
  controls(true); $("activity").dataset.result = "working";
  try {
    selectedPort = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a, usbProductId: 0x1001 }] });
    status("Setting your local time and checking the badge…", 90);
    await syncClock();
  } catch (error) { $("activity").dataset.result = "error"; status(error.name === "NotFoundError" ? "No device selected." : error.message); }
  finally { controls(false); }
});
$("hardware").addEventListener("change", () => controls(false));
$("copy").addEventListener("click", async () => {
  if (!diagnostics) return;
  try { await navigator.clipboard.writeText(JSON.stringify(diagnostics, null, 2)); $("copy").textContent = "Copied"; }
  catch { $("details").textContent = JSON.stringify(diagnostics, null, 2); $("details").hidden = false; }
});
$("report").addEventListener("click", () => {
  const body = "## What happened?\n\n\n## What did you expect?\n\n\n## Steps to reproduce\n\n\n## Badge details\n\n```json\n" + JSON.stringify(diagnostics || { release: "conference-factory-3", status: "Not checked in this browser" }, null, 2) + "\n```\n\nBrowser / operating system:\nOrientation:\n\nPlease omit Wi-Fi passwords, personal photos, and raw device logs.";
  $("report").href = "https://github.com/chantastic/m5stack-stopwatch-authkit/issues/new?" + new URLSearchParams({ title: "[Badge preview] ", body });
});
const supported = window.isSecureContext && "serial" in navigator && window.top === window.self;
if (!supported) {
  $("unsupported").hidden = false;
  $("hardware").disabled = true; $("install").disabled = true; $("clock").disabled = true;
} else controls(false);
window.addEventListener("beforeunload", event => { if (busy) { event.preventDefault(); event.returnValue = ""; } });
