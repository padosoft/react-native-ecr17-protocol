/**
 * React Native Windows autolinking for @padosoft/react-native-ecr17.
 *
 * The package's own `react-native.config.js` already points RNW at
 * `windows/Ecr17.sln`. Apps still need this helper because
 * `react-native-nitro-modules` has no Windows project yet
 * (https://github.com/mrousavy/nitro/issues/168). This package's Windows DLL
 * implements `NitroModules.install()` and registers its HybridObjects itself.
 *
 * Spread into the app's `react-native.config.js`:
 *
 *   const { windowsAppDependencies } = require('@padosoft/react-native-ecr17/windows-autolink');
 *   module.exports = {
 *     dependencies: windowsAppDependencies(),
 *   };
 *
 * Pass `{ root }` only when the package is linked from a local checkout
 * (`file:` / `link:` / monorepo), so the CLI finds it.
 */

const windowsNativeProject = {
	sourceDir: ".\\windows",
	solutionFile: "Ecr17.sln",
	projects: [
		{
			projectFile: "Ecr17\\Ecr17.vcxproj",
			directDependency: true,
		},
	],
};

function windowsAppDependencies(options = {}) {
	const dependencies = {
		// Skip Nitro's (missing) Windows project; Ecr17.dll installs Nitro.
		"react-native-nitro-modules": {
			platforms: {
				windows: null,
			},
		},
	};

	if (options.root) {
		dependencies["@padosoft/react-native-ecr17"] = { root: options.root };
	}

	return dependencies;
}

module.exports = {
	windowsNativeProject,
	windowsAppDependencies,
};
