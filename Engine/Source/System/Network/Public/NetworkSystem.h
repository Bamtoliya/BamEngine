#pragma once

#include "System.h"
#include "NetworkDriver.h"
#include "Components/Network/NetworkIdentityComponent.h"
#include "Components/Network/NetworkTransformComponent.h"
#include <memory>
#include <unordered_map>

BEGIN(Engine)

class ENGINE_API NetworkSystem : public ISystem
{
private:
    NetworkSystem() = default;
    virtual ~NetworkSystem() override = default;
    EResult Initialize(void* arg = nullptr);

public:
    static NetworkSystem* Create(void* arg = nullptr);
    virtual void Free() override;

public:
    virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
    virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::Gameplay); }

    // 네트워크 드라이버 접근자
    Network::NetworkDriver& GetDriver() { return *m_Driver; }
    const Network::NetworkDriver& GetDriver() const { return *m_Driver; }

    // NetId <-> entt::entity 등록 및 조회
    void RegisterEntity(Network::NetId netId, entt::entity entity);
    void UnregisterEntity(Network::NetId netId);
    entt::entity FindEntityByNetId(Network::NetId netId) const;

    // 서버 전용: 엔티티에 고유 NetId 자동 발급
    Network::NetId AssignNetId(entt::registry& registry, entt::entity entity, Network::ClientId ownerClientId = Network::ServerClientId);

private:
    void RegisterNetworkHandlers(entt::registry& registry);
    void UpdateServerSync(entt::registry& registry, f32 dt);
    void UpdateClientInterpolation(entt::registry& registry, f32 dt);

    void HandleSyncTransform(const Network::PacketHeader& header, Network::PacketReader& reader, entt::registry& registry);

private:
    std::unique_ptr<Network::NetworkDriver> m_Driver;
    std::unordered_map<Network::NetId, entt::entity> m_NetIdToEntity;
    Network::NetId m_NextServerNetId = 1;
};

END