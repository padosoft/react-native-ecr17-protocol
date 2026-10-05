// React Native CLI config for the Windows example.
//
// `windowsAppDependencies()` skips react-native-nitro-modules' (missing) Windows
// project: the @padosoft/react-native-ecr17 DLL installs Nitro itself.
const {
	windowsAppDependencies,
} = require("@padosoft/react-native-ecr17/windows-autolink");

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
