// swift-tools-version:5.9
//
// Ecr17: the C++ protocol core of @padosoft/ecr17 (packages/ecr17) as a Swift package.
//
// SwiftPM resolves a package from the root of its git repo, by plain semver tag, so
// this manifest lives here and the release workflow tags each @padosoft/ecr17 version
// as `X.Y.Z` (the old 1.x tags predate it; @padosoft/ecr17 starts at 2.0.0).
//
// @padosoft/react-native-ecr17's podspec links this package (native_dependency) when
// the app switches SwiftPM on, else the Ecr17 pod. Native apps can add it directly:
//
//   .package(url: "https://github.com/padosoft/react-native-ecr17-protocol.git", from: "2.0.0")
//
// The headers are C++ (`#include <ecr17/Ecr17Client.hpp>`), the same as the pod and CMake.
import PackageDescription

let package = Package(
  name: "Ecr17",
  platforms: [
    .iOS(.v15),
    .macOS(.v12),
    .visionOS(.v1),
  ],
  products: [
    .library(name: "Ecr17", targets: ["Ecr17"]),
  ],
  targets: [
    .target(
      name: "Ecr17",
      path: "packages/ecr17/cpp",
      exclude: ["tests"],
      sources: ["src"],
      publicHeadersPath: "include"
    ),
  ],
  cxxLanguageStandard: .cxx20
)
