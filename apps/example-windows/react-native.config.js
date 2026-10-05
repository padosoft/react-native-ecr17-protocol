// React Native CLI config for the Windows example.
//
// `windowsAppDependencies()` skips react-native-nitro-modules' (missing) Windows
// project: the app's Nitro host, @padosoft/react-native-nitro-windows, installs
// Nitro and compiles @padosoft/react-native-ecr17's Windows C++ (and the Kit's).
const {
	windowsAppDependencies,
} = require("@padosoft/react-native-nitro-windows");

module.exports = {
	project: {
		windows: {
			sourceDir: "windows",
			solutionFile: "Ecr17Example.sln",
			project: {
				projectFile: "Ecr17Example\\Ecr17Example.vcxproj",
			},
		},
	},
	dependencies: windowsAppDependencies(),
};
