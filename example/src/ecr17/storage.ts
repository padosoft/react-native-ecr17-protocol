// Persists the last-used Ecr17Config to a JSON file in the document directory so
// the debug screen doesn't have to be re-filled each launch.

import { File, Paths } from 'expo-file-system';
import type { Ecr17Config } from 'react-native-ecr17';

const CONFIG_FILE = 'ecr17-config.json';

export const DEFAULT_CONFIG: Ecr17Config = {
  host: '',
  port: 10000,
  terminalId: '',
  cashRegisterId: '',
  lrcMode: 'std',
  keepAlive: true,
  autoReconnect: true,
  connectionTimeoutMs: 5000,
  responseTimeoutMs: 60000,
  ackTimeoutMs: 2000,
  retryCount: 3,
  retryDelayMs: 200,
  receiptDrainMs: 0,
  debug: true,
};

function configFile(): File {
  return new File(Paths.document, CONFIG_FILE);
}

export async function loadConfig(): Promise<Ecr17Config> {
  try {
    const file = configFile();
    if (!file.exists) {
      return { ...DEFAULT_CONFIG };
    }
    const parsed = JSON.parse(file.textSync()) as Partial<Ecr17Config>;
    return { ...DEFAULT_CONFIG, ...parsed };
  } catch {
    return { ...DEFAULT_CONFIG };
  }
}

export function saveConfig(config: Ecr17Config): void {
  try {
    configFile().write(JSON.stringify(config));
  } catch {
    // best-effort persistence
  }
}
