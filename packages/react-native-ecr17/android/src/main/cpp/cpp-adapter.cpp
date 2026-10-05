#include <jni.h>
#include <fbjni/fbjni.h>
#include "Ecr17OnLoad.hpp"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::jni::initialize(vm, []() {
    margelo::nitro::ecr17::registerAllNatives();
  });
}