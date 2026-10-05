// CI helper: drops the @padosoft tooling packages that live on the private GitHub
// Packages registry (and are not published on npm yet) from every manifest of this
// repo, so `bun install` needs no registry auth. The workspace packages
// (@padosoft/ecr17-kit, @padosoft/react-native-ecr17) are kept.
//
//   node scripts/strip-private-deps.mjs [--keep @padosoft/native-modules,...]
import { readFileSync, writeFileSync } from "node:fs";

const workspace = new Set([
	"@padosoft/ecr17-kit",
	"@padosoft/react-native-ecr17",
]);
const keepIndex = process.argv.indexOf("--keep");
const keep = new Set(
	keepIndex > 0 ? process.argv[keepIndex + 1].split(",") : [],
);
const manifests = [
	"package.json",
	"kit/package.json",
	"package/package.json",
	"example/package.json",
];

for (const path of manifests) {
	const json = JSON.parse(readFileSync(path, "utf8"));
	const removed = [];
	for (const field of [
		"dependencies",
		"devDependencies",
		"peerDependencies",
		"peerDependenciesMeta",
	]) {
		for (const name of Object.keys(json[field] ?? {})) {
			if (
				name.startsWith("@padosoft/") &&
				!workspace.has(name) &&
				!keep.has(name)
			) {
				delete json[field][name];
				removed.push(name);
			}
		}
	}
	writeFileSync(path, `${JSON.stringify(json, null, "\t")}\n`);
	if (removed.length)
		console.log(`${path}: removed ${[...new Set(removed)].join(", ")}`);
}
