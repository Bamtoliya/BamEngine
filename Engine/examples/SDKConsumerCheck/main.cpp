#include "ResourceManager.h"

#include <iostream>

int main()
{
    std::cout << "BAM_EDITOR=" << BAM_EDITOR << '\n';
    std::cout << "BAM_SHIPPING=" << BAM_SHIPPING << '\n';

#ifdef _DEBUG
    std::cout << "Configuration=Debug\n";
#else
    std::cout << "Configuration=Release\n";
#endif

    if (!Engine::ResourceManager::Create())
        return 1;

    auto& manager = Engine::ResourceManager::Get();

    const auto metrics = manager.GetMetrics();

    std::cout << "Active resources: " << metrics.ActiveResources << '\n';

    Engine::ResourceManager::Destroy();

    return 0;
}