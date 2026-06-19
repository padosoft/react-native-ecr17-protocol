import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

const required = [
  "_site/index.html",
  "_site/llms.txt",
  "_site/sitemap.xml"
];

const failures = [];
for (const file of required) {
  if (!existsSync(file)) failures.push(`Missing ${file}`);
}

if (existsSync("_site")) {
  const htmlFiles = [];
  const walk = (dir) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) walk(path);
      if (entry.isFile() && entry.name.endsWith(".html")) htmlFiles.push(path);
    }
  };
  walk("_site");
  const leaked = htmlFiles.filter((file) => readFileSync(file, "utf8").includes(":::"));
  if (leaked.length) failures.push(`Leaked docmd container markers: ${leaked.join(", ")}`);
  const html = htmlFiles.map((file) => readFileSync(file, "utf8")).join("\n");
  if (!html.includes("katex")) failures.push("KaTeX output was not found");
  if (!html.includes("mermaid")) failures.push("Mermaid output was not found");
}

if (failures.length > 0) {
  console.error(failures.join("\n"));
  process.exit(1);
}

console.log("Build output guard passed.");
