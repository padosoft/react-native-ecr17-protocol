// CI helper: drops the @padosoft tooling packages that live on the private GitHub
// Packages registry (and are not published on npm yet) from every manifest of this
// repo, so `bun install` needs no registry auth. The workspace packages
// (@padosoft/ecr17, @padosoft/react-native-ecr17) are kept.
//
//   bun scripts/strip-private-deps.ts [--keep @padosoft/native-modules,...]
import { readFileSync, writeFileSync } from "node:fs";

type Manifest = Record<string, unknown> &
	Partial<Record<DependencyField, Record<string, unknown>>>;
type DependencyField =
	| "dependencies"
	| "devDependencies"
	| "peerDependencies"
	| "peerDependenciesMeta";

const workspace = new Set(["@padosoft/ecr17", "@padosoft/react-native-ecr17"]);
const keepIndex = process.argv.indexOf("--keep");
const keep = new Set(
	keepIndex > 0 ? (process.argv[keepIndex + 1] ?? "").split(",") : [],
);
const manifests = [
	"package.json",
	"packages/ecr17/package.json",
	"packages/react-native-ecr17/package.json",
	"apps/example/package.json",
];
const fields: DependencyField[] = [
	"dependencies",
	"devDependencies",
	"peerDependencies",
	"peerDependenciesMeta",
];

for (const path of manifests) {
	const json = JSON.parse(readFileSync(path, "utf8")) as Manifest;
	const removed = new Set<string>();
	for (const field of fields) {
		const entries = json[field];
		if (!entries) continue;
		for (const name of Object.keys(entries)) {
			if (
				name.startsWith("@padosoft/") &&
				!workspace.has(name) &&
				!keep.has(name)
			) {
				delete entries[name];
				removed.add(name);
			}
		}
	}
	writeFileSync(path, `${JSON.stringify(json, null, "\t")}\n`);
	if (removed.size) console.log(`${path}: removed ${[...removed].join(", ")}`);
}
