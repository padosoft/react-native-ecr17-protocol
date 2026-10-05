// Keep the root README.md in sync with packages/react-native-ecr17/README.md.
//
// GitHub only renders a README that lives in the repo root, so the polished
// package README (the one published to npm) is mirrored verbatim to the root to
// show up on the repository home page. The package README uses absolute URLs
// for every badge/image/link precisely because it ships to npm — which means
// the two files are byte-identical and the mirror is a plain copy, no path
// rewriting.
//
//   bun scripts/sync-readme.ts          regenerate the root README
//   bun scripts/sync-readme.ts --check  fail (exit 1) if it is out of date
//
// The --check form runs in CI (ts-checks) so a stale root README never lands.

import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const source = join(root, "packages", "react-native-ecr17", "README.md");
const target = join(root, "README.md");

const wanted = readFileSync(source, "utf8");
const check = process.argv.includes("--check");

if (check) {
	let current = "";
	try {
		current = readFileSync(target, "utf8");
	} catch {
		// missing target: treated as out of date below
	}
	if (current !== wanted) {
		console.error(
			"README.md is out of sync with packages/react-native-ecr17/README.md.\n" +
				"Run `bun run sync:readme` and commit the result.",
		);
		process.exit(1);
	}
	console.log(
		"README.md is in sync with packages/react-native-ecr17/README.md.",
	);
} else {
	writeFileSync(target, wanted);
	console.log("Synced README.md from packages/react-native-ecr17/README.md.");
}
