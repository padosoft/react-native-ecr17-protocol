# The Ecr17 pod (iOS/visionOS): the C++ protocol core of @padosoft/ecr17. Headers are
# public under <ecr17/...>, as in the CMake build and the Swift package (Package.swift
# at the repo root). native_kit fills the metadata and logs the build mode; there is no
# prebuilt xcframework, so it always compiles the sources.
require File.join(
  File.dirname(`node --print "require.resolve('@padosoft/native-modules/package.json')"`.strip),
  "ruby/helpers",
)
require_from_node_modules("@padosoft/native-modules", "ruby/native_kit")

Pod::Spec.new do |s|
  native_kit(
    s,
    dir: __dir__,
    name: "Ecr17",
    source_files: ["cpp/include/**/*.hpp", "cpp/src/**/*.cpp"],
    public_header_files: "cpp/include/**/*.hpp",
  ) do
    s.header_mappings_dir = "cpp/include"
    # A Swift pod (the Nitro module) depends on this one, so it must define a module
    # (DEFINES_MODULE: CocoaPods generates the module map). Its headers are C++: only
    # C++ / Objective-C++ code includes them.
    s.pod_target_xcconfig = {
      "CLANG_CXX_LANGUAGE_STANDARD" => "c++20",
      "DEFINES_MODULE" => "YES",
      "HEADER_SEARCH_PATHS" => '"$(PODS_TARGET_SRCROOT)/cpp/include"',
    }
  end
end
