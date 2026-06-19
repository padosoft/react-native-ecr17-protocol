import { readFile, readdir } from "node:fs/promises";
import { join } from "node:path";

const docsDir = "docs";
const rawHtml = /<\/?[a-z][\w:-]*(?:\s[^>]*)?>/i;
const failures = [];

async function walk(dir) {
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) {
      await walk(path);
    } else if (entry.isFile() && entry.name.endsWith(".md")) {
      await check(path);
    }
  }
}

async function check(path) {
  const lines = (await readFile(path, "utf8")).split(/\r?\n/);
  let fenced = false;
  lines.forEach((line, index) => {
    if (/^\s*(```|~~~)/.test(line)) {
      fenced = !fenced;
      return;
    }
    const withoutInlineCode = line.replace(/`[^`]*`/g, "");
    if (!fenced && rawHtml.test(withoutInlineCode)) {
      failures.push(`${path}:${index + 1}: raw HTML is not allowed`);
    }
  });
}

await walk(docsDir);

if (failures.length > 0) {
  console.error(failures.join("\n"));
  process.exit(1);
}

console.log("Markdown raw HTML guard passed.");
