// React Native CLI autolinking config for this native module.
// https://github.com/react-native-community/cli/blob/main/docs/dependencies.md
//
// Declaring the android/ios platforms makes the CLI treat this package as a
// native dependency and autolink it. On Android that registers Ecr17Package,
// whose static initializer loads libEcr17.so (System.loadLibrary) so the Nitro
// HybridObjects are registered before JS creates them.

module.exports = {
  dependency: {
    platforms: {
      /** @type {import('@react-native-community/cli-types').IOSDependencyParams} */
      ios: {},
      /** @type {import('@react-native-community/cli-types').AndroidDependencyParams} */
      android: {},
    },
  },
};
