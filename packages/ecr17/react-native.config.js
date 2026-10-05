// @padosoft/ecr17 is not a React Native module, so autolinking skips it. The Ecr17 pod
// is declared by the app instead: by this package's Expo config plugin (app.plugin.js),
// or by one Podfile line in a bare app. That keeps it out of the app when SwiftPM is on,
// where ReactNativeEcr17 links Ecr17 as a Swift package: autolinking would add the pod
// regardless. Android and Windows compile the sources through the binding's own build.
module.exports = {
	dependency: {
		platforms: {
			ios: null,
			android: null,
			windows: null,
		},
	},
};
