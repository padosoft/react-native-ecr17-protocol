// Expo config plugin: adds the Ecr17Kit pod to the app's Podfile after `expo prebuild`
// (iOS). Android needs nothing: @padosoft/react-native-ecr17's CMake compiles the
// Kit's sources. Uses @padosoft/expo's createNativeKitPlugin, like every Padosoft Kit.
const {
	createNativeKitPlugin,
} = require("@padosoft/expo/plugins/withNativeKit");

module.exports = createNativeKitPlugin({
	packageName: "@padosoft/ecr17-kit",
	podName: "Ecr17Kit",
	marker: "@padosoft/withEcr17Kit",
	gradleName: "ecr17-kit",
});
