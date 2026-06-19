import { cp, mkdir, rm } from "node:fs/promises";
import { existsSync } from "node:fs";

await mkdir("_site", { recursive: true });

if (existsSync("_site/.docmd-search")) {
  await rm("_site/.docmd-search", { recursive: true, force: true });
}

await cp("docs/.docmd-search", "_site/.docmd-search", { recursive: true, force: true });
