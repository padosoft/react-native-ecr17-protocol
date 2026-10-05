# The whole podspec comes from @padosoft/native-modules' nitro_module: metadata from
# package.json, the module's sources plus nitrogen's, React-jsi/callinvoker. The pod is
# ReactNativeEcr17 (nitro.json's iosModuleName), so the protocol core keeps the name Ecr17.
#
# The core (@padosoft/ecr17) is linked through native_dependency: as the Ecr17 Swift
# package (Package.swift at this repo's root) when the app switches SwiftPM on
# (@padosoft/expo's withSwiftPackageManager), else as the Ecr17 pod. The SwiftPM
# requirement follows this package's dependency range on @padosoft/ecr17, so both
# paths resolve the same versions.
require File.join(
  File.dirname(`node --print "require.resolve('@padosoft/native-modules/package.json')"`.strip),
  "ruby/helpers",
)
require_from_node_modules("@padosoft/native-modules", "ruby/nitro_module", "ruby/native_dependency")

ecr17_version = package_json(__dir__).dig("dependencies", "@padosoft/ecr17").to_s[/\d+\.\d+\.\d+/]

Pod::Spec.new do |s|
  nitro_module(s, dir: __dir__, name: "ReactNativeEcr17") do
    native_dependency(
      s, "Ecr17",
      spm_url: "https://github.com/padosoft/react-native-ecr17-protocol.git",
      spm_requirement: { kind: "upToNextMajorVersion", minimumVersion: ecr17_version },
      spm_products: ["Ecr17"],
    )
    # nitrogen already set pod_target_xcconfig: add to it, don't replace it.
    xcconfig = s.attributes_hash["pod_target_xcconfig"] || {}
    s.pod_target_xcconfig = xcconfig.merge(
      "HEADER_SEARCH_PATHS" => [xcconfig["HEADER_SEARCH_PATHS"], '"$(PODS_TARGET_SRCROOT)/cpp"'].compact.join(" "),
    )
  end
end
