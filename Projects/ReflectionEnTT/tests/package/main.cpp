#include "Package.gen.h"
#include "Types.h"
#include <reflection_entt/Reflection.h>

int main()
{
    using namespace entt::literals;
    entt::meta_ctx context;
    if (!reflection_entt_generated::Register_Package(context)) return 1;
    package_example::Settings settings;
    const auto property = entt::resolve<package_example::Settings>(context).data("Speed"_hs);
    auto instance = entt::forward_as_meta(context, settings);
    const auto* metadata = reflection_entt::GetMetadata(property);
    const auto* name = metadata ? metadata->FindAs<std::string>("DisplayName") : nullptr;
    return property.set(instance, 12.0f) && settings.Speed == 12.0f && name && *name == "Speed" ? 0 : 2;
}
