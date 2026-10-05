# The whole podspec comes from @padosoft/native-modules' nitro_module: metadata from
# package.json, the module's sources plus nitrogen's, React-jsi/callinvoker and the
# dependency on the protocol core, the Ecr17Kit pod (@padosoft/ecr17-kit).
require File.join(
  File.dirname(`node --print "require.resolve('@padosoft/native-modules/package.json')"`.strip),
  "ruby/helpers",
)
require_from_node_modules("@padosoft/native-modules", "ruby/nitro_module")

Pod::Spec.new do |s|
  nitro_module(s, dir: __dir__, name: "Ecr17", kit: "Ecr17Kit") do
    # nitrogen already set pod_target_xcconfig: add to it, don't replace it.
    xcconfig = s.attributes_hash["pod_target_xcconfig"] || {}
    s.pod_target_xcconfig = xcconfig.merge(
      "HEADER_SEARCH_PATHS" => [xcconfig["HEADER_SEARCH_PATHS"], '"$(PODS_TARGET_SRCROOT)/cpp"'].compact.join(" "),
    )
  end
end
