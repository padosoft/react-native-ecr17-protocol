import { NativeTabs } from "expo-router/unstable-native-tabs";
import { useColorScheme } from "react-native";

import { Colors } from "@/constants/theme";

export default function AppTabs() {
	const scheme = useColorScheme();
	const colors = Colors[scheme === "unspecified" ? "light" : scheme];

	return (
		<NativeTabs
			backgroundColor={colors.background}
			iconColor={{ selected: colors.text }}
			indicatorColor={colors.backgroundElement}
			labelStyle={{ selected: { color: colors.text } }}
		>
			<NativeTabs.Trigger name="index">
				<NativeTabs.Trigger.Label>Home</NativeTabs.Trigger.Label>
				<NativeTabs.Trigger.Icon md={"build"} sf={"wrench"} />
			</NativeTabs.Trigger>

			<NativeTabs.Trigger name="explore">
				<NativeTabs.Trigger.Label>Logs</NativeTabs.Trigger.Label>
				<NativeTabs.Trigger.Icon md={"text_snippet"} sf={"doc.text"} />
			</NativeTabs.Trigger>
		</NativeTabs>
	);
}
