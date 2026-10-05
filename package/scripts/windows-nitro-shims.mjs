#!/usr/bin/env node

/**
 * Generates windows/include/NitroModules/*.hpp for the React Native Windows build.
 *
 * Nitro and nitrogen-generated C++ include `<NitroModules/Foo.hpp>`. iOS and
 * Android get that prefix from a header map / prefab; MSVC has none. This emits
 * one-line shims that include the real header by basename (Ecr17.vcxproj puts
 * every react-native-nitro-modules cpp/ subdirectory on the include path).
 *
 * Run by Ecr17.vcxproj before compiling, so `--ignore-scripts` installs still build:
 *   node scripts/windows-nitro-shims.mjs --nitro <react-native-nitro-modules dir> --out <dir>
 *   node scripts/windows-nitro-shims.mjs            (auto-detects, writes to windows/include/NitroModules)
 *   node scripts/windows-nitro-shims.mjs --optional (exits 0 when Nitro is not installed)
 */

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const packageRoot = path.resolve(
	path.dirname(fileURLToPath(import.meta.url)),
	"..",
);

function cleanPath(value) {
	if (!value) {
		return "";
	}
	// MSBuild `"$(Dir)\"` treats the trailing backslash as an escape, so argv can
	// end with a literal quote. The vcxproj passes `"$(Dir)."`, which leaves `\.`.
	return value.replace(/["']+$/g, "").replace(/[\\/]+(\.)?$/, "");
}

function parseArgs(argv) {
	const args = { nitro: "", out: "", optional: false };
	for (let i = 0; i < argv.length; i += 1) {
		const arg = argv[i];
		if (arg === "--optional") {
			args.optional = true;
		} else if (arg === "--nitro") {
			args.nitro = cleanPath(argv[i + 1]);
			i += 1;
		} else if (arg === "--out") {
			args.out = cleanPath(argv[i + 1]);
			i += 1;
		}
	}
	return args;
}

function findNitroRoot(explicit, optional) {
	if (explicit) {
		return path.resolve(explicit);
	}
	// Walk up from the package (covers app node_modules, hoisted monorepos and this repo).
	let dir = packageRoot;
	while (true) {
		const candidate = path.join(
			dir,
			"node_modules",
			"react-native-nitro-modules",
		);
		if (fs.existsSync(path.join(candidate, "package.json"))) {
			return candidate;
		}
		const parent = path.dirname(dir);
		if (parent === dir) {
			break;
		}
		dir = parent;
	}
	if (optional) {
		return null;
	}
	throw new Error("react-native-nitro-modules not found. Pass --nitro <dir>.");
}

function collectHeaders(cppRoot) {
	const byName = new Map();
	const stack = [cppRoot];
	while (stack.length > 0) {
		const dir = stack.pop();
		for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
			const full = path.join(dir, entry.name);
			if (entry.isDirectory()) {
				stack.push(full);
				continue;
			}
			if (!entry.name.endsWith(".hpp")) {
				continue;
			}
			const existing = byName.get(entry.name);
			if (existing && existing !== full) {
				console.warn(
					`skip duplicate ${entry.name}: ${full} (keeping ${existing})`,
				);
				continue;
			}
			byName.set(entry.name, full);
		}
	}
	return [...byName.keys()].sort();
}

function writeShims(outDir, names) {
	fs.mkdirSync(outDir, { recursive: true });
	for (const name of fs.readdirSync(outDir)) {
		if (name.endsWith(".hpp")) {
			fs.unlinkSync(path.join(outDir, name));
		}
	}
	for (const name of names) {
		const stem = name.slice(0, -".hpp".length);
		fs.writeFileSync(
			path.join(outDir, name),
			`#pragma once\n#include <${stem}.hpp>\n`,
			"utf8",
		);
	}
}

const args = parseArgs(process.argv.slice(2));
const nitroRoot = findNitroRoot(args.nitro, args.optional);
if (!nitroRoot) {
	console.warn(
		"skip Nitro Windows include shims: react-native-nitro-modules is not installed",
	);
	process.exit(0);
}
const cppRoot = path.join(nitroRoot, "cpp");
if (!fs.existsSync(cppRoot)) {
	throw new Error(`No cpp/ folder in ${nitroRoot}`);
}

const outDir = args.out
	? path.resolve(args.out)
	: path.join(packageRoot, "windows", "include", "NitroModules");

const names = collectHeaders(cppRoot);
writeShims(outDir, names);
console.log(`wrote ${names.length} Nitro include shims to ${outDir}`);
