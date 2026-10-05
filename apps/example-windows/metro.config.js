const { getDefaultConfig, mergeConfig } = require("@react-native/metro-config");

const fs = require("node:fs");
const path = require("node:path");

const projectRoot = __dirname;
const librarySrc = path.resolve(projectRoot, "../../packages/react-native-ecr17/src");
const libraryName = "@padosoft/react-native-ecr17";

// The RN CLI installs the `react-native` -> `react-native-windows` redirect on
// the default config before this file runs. Keep it and delegate to it.
const defaultConfig = getDefaultConfig(projectRoot);
const baseResolveRequest = defaultConfig.resolver.resolveRequest;

const rnwPath = fs.realpathSync(
	path.resolve(require.resolve("react-native-windows/package.json"), ".."),
);

// Bare imports made from the library sources (packages/react-native-ecr17/src) must
// resolve from THIS app's node_modules. Otherwise Metro walks up from the library and finds the
// repo root node_modules, which holds the Expo example's react-native (a
// different version) and a second copy of react-native-nitro-modules.
const appOrigin = path.join(projectRoot, "index.js");

function isBareSpecifier(moduleName) {
	return !moduleName.startsWith(".") && !path.isAbsolute(moduleName);
}

/**
 * Metro configuration
 * https://facebook.github.io/metro/docs/configuration
 *
 * @type {import('metro-config').MetroConfig}
 */
const config = {
	watchFolders: [librarySrc],
	resolver: {
		blockList: [
			// Stops `run-windows` from crashing an already running Metro server.
			new RegExp(
				`${path.resolve(projectRoot, "windows").replace(/[/\\]/g, "/")}.*`,
			),
			// Avoids EBUSY on msbuild.ProjectImports.zip and other MSBuild output.
			new RegExp(`${rnwPath}/build/.*`),
			new RegExp(`${rnwPath}/target/.*`),
			/.*\.ProjectImports\.zip/,
		],
		resolveRequest: (context, moduleName, platform) => {
			// Use the library's TypeScript sources directly (no build step needed).
			if (moduleName === libraryName) {
				return {
					type: "sourceFile",
					filePath: path.join(librarySrc, "index.ts"),
				};
			}
			const fromLibrary = context.originModulePath.startsWith(librarySrc);
			const ctx =
				fromLibrary && isBareSpecifier(moduleName)
					? { ...context, originModulePath: appOrigin }
					: context;
			return baseResolveRequest
				? baseResolveRequest(ctx, moduleName, platform)
				: ctx.resolveRequest(ctx, moduleName, platform);
		},
	},
	transformer: {
		getTransformOptions: async () => ({
			transform: {
				experimentalImportSupport: false,
				inlineRequires: true,
			},
		}),
	},
};

module.exports = mergeConfig(defaultConfig, config);
