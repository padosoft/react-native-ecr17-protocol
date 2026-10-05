package com.padosoft.ecr17

import com.facebook.react.BaseReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.model.ReactModuleInfoProvider
import com.margelo.nitro.ecr17.Ecr17OnLoad

/**
 * Autolinked ReactPackage for the ECR17 Nitro module.
 *
 * It exposes no TurboModules (Nitro registers its HybridObjects via the C++
 * registry, not the RN module system). Its sole job is to load the native
 * "Ecr17" C++ library when the package is constructed by React Native's
 * autolinking, so the HybridObjects are registered before JS creates them.
 */
class Ecr17Package : BaseReactPackage() {
  override fun getModule(name: String, reactContext: ReactApplicationContext): NativeModule? = null

  override fun getReactModuleInfoProvider(): ReactModuleInfoProvider =
    ReactModuleInfoProvider { emptyMap() }

  companion object {
    init {
      // Idempotent: loads libEcr17.so (System.loadLibrary), which runs
      // JNI_OnLoad -> registerAllNatives() and registers the HybridObjects.
      Ecr17OnLoad.initializeNative()
    }
  }
}
