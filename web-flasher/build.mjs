import { build } from "esbuild";
import { mkdir, copyFile, readFile, writeFile } from "node:fs/promises";
import { resolve } from "node:path";

const out = resolve(process.env.WEB_INSTALLER_OUT || "../.build/web-installer");
await mkdir(out, { recursive: true });
await build({ entryPoints: ["src/installer.js"], bundle: true, format: "esm", target: "chrome120", outfile: `${out}/installer.js`, minify: true, legalComments: "eof" });
for (const file of ["index.html", "style.css"]) await copyFile(file, `${out}/${file}`);
await writeFile(`${out}/THIRD_PARTY_NOTICES.txt`, [
  "StopWatch browser installer dependencies\n",
  "esptool-js 0.6.1 — Espressif Systems, Apache-2.0\n" + await readFile("licenses/esptool-js.txt", "utf8"),
  "js-md5 0.8.3 — Chen, Yi-Cyuan, MIT\n" + await readFile("node_modules/js-md5/LICENSE.txt", "utf8"),
  "pako — Vitaly Puzrin and Andrey Tupitsin, MIT / Zlib\n" + await readFile("node_modules/pako/LICENSE", "utf8"),
  "tslib — Microsoft Corporation, BSD-0-Clause\n" + await readFile("node_modules/tslib/LICENSE.txt", "utf8"),
  "atob-lite — Mathias Bynens and contributors, MIT\n" + await readFile("node_modules/atob-lite/LICENSE.md", "utf8")
].join("\n\n"));
console.log(`Installer bundled into ${out}`);
