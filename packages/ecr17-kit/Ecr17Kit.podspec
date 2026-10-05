# Ecr17Kit for iOS/visionOS: the C++ protocol core, compiled from source (no prebuilt).
# Headers are public under <Ecr17Kit/...>, as in the CMake build.
require File.join(
  File.dirname(`node --print "require.resolve('@padosoft/native-modules/package.json')"`.strip),
  "ruby/helpers",
)
require_from_node_modules("@padosoft/native-modules", "ruby/podspec_metadata")

Pod::Spec.new do |s|
  PodspecMetadata.apply(s, package_json(__dir__), name: "Ecr17Kit")

  s.source_files = ["cpp/include/**/*.hpp", "cpp/src/**/*.cpp"]
  s.public_header_files = "cpp/include/**/*.hpp"
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
