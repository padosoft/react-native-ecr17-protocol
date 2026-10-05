#include "pch.h"

#include "ReactPackageProvider.h"
#if __has_include("ReactPackageProvider.g.cpp")
#include "ReactPackageProvider.g.cpp"
#endif

#include "Ecr17Module.h"

using namespace winrt::Microsoft::ReactNative;

namespace winrt::Ecr17::implementation
{

void ReactPackageProvider::CreatePackage(IReactPackageBuilder const &packageBuilder) noexcept
{
  AddAttributedModules(packageBuilder, true);
}

} // namespace winrt::Ecr17::implementation
