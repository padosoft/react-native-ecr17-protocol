import { NitroModules } from 'react-native-nitro-modules'
import type { Ecr17 as Ecr17Spec } from './specs/ecr17.nitro'

export const Ecr17 =
  NitroModules.createHybridObject<Ecr17Spec>('Ecr17')