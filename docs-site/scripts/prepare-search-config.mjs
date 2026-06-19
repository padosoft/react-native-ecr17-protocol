import { copyFile, mkdir } from "node:fs/promises";

await mkdir("docs/.docmd-search", { recursive: true });
await copyFile(".docmd-search/config.json", "docs/.docmd-search/config.json");
