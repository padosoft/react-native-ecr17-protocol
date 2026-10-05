import { NitroModules } from "react-native-nitro-modules";
import type { Ecr17Client, Ecr17Config } from "../specs/client.nitro";

export function createEcr17Client(config: Ecr17Config): Ecr17Client {
	const client = NitroModules.createHybridObject<Ecr17Client>("Ecr17Client");

	client.configure(config);

	return client;
}
