import { tsdown } from "@padosoft/config/compiler/tsdown";

// The Node.js entry (src/index.ts). The addon itself (ecr17.node) is not bundled: src/native.ts
// loads it at runtime from build/Release or prebuilds/<platform>-<arch>.
export default tsdown({
	entry: ["src/index.ts"],
	format: ["esm", "cjs"],
	platform: "node",
	// import.meta.url (the addon lookup) in the CommonJS build.
	shims: true,
});
