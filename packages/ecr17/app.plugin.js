// Expo config plugin: adds the Ecr17 pod (the C++ core) to the app's Podfile after
// `expo prebuild` (iOS). Android needs nothing: @padosoft/react-native-ecr17's CMake
// compiles the sources.
//
// With SwiftPM switched on (@padosoft/expo's withSwiftPackageManager writes
// $NativeKitForceSPM = true), ReactNativeEcr17 links Ecr17 as a Swift package
// (Package.swift at the repo root, through @padosoft/native-modules'
// native_dependency), so the pod is skipped there: the same rule native_dependency
// applies, evaluated when `pod install` runs.
const path = require("node:path");
const { withPodfile } = require("expo/config-plugins");

const MARKER = "@padosoft/withEcr17";

function podLines(dir) {
	return [
		"# Skipped when SwiftPM is on: ReactNativeEcr17 then links Ecr17 as a Swift package.",
		"ecr17_spm = [$NativeKitForceSPM, $PadosoftForceSPM].any? && ![$NativeKitDisableSPM, $PadosoftDisableSPM].any?",
		`pod 'Ecr17', :path => '${dir}' unless ecr17_spm`,
	];
}

// Replaces any previous block (idempotent across prebuilds), then inserts it at the
// top of every `target '…' do`.
function addPod(contents, dir) {
	const previous = new RegExp(
		`\\n?[ \\t]*# ${MARKER}:start[\\s\\S]*?# ${MARKER}:end[^\\n]*\\n?`,
		"g",
	);
	const block = [`# ${MARKER}:start`, ...podLines(dir), `# ${MARKER}:end`]
		.map((line) => `  ${line}\n`)
		.join("");
	return contents
		.replace(previous, "")
		.replace(
			/(target\s+['"][^'"]+['"]\s+do\b[^\n]*\n)/g,
			(target) => `${target}${block}`,
		);
}

module.exports = (config) =>
	withPodfile(config, (podfile) => {
		const dir = path.dirname(require.resolve("@padosoft/ecr17/package.json"));
		podfile.modResults.contents = addPod(podfile.modResults.contents, dir);
		return podfile;
	});
