#include "NetworkSystem.h"
#include "Components/Core/TransformComponent.h"
#include <algorithm>

BEGIN(Engine)

NetworkSystem* NetworkSystem::Create(void* arg)
{
    NetworkSystem* instance = new NetworkSystem();
    if (instance->Initialize(arg) != EResult::Success)
    {
        delete instance;
        return nullptr;
    }
    return instance;
}

void NetworkSystem::Free()
{
    if (m_Driver)
    {
        m_Driver->Stop(Network::EDisconnectReason::Requested);
        m_Driver.reset();
    }
    m_NetIdToEntity.clear();
}

EResult NetworkSystem::Initialize(void* arg)
{
    m_Driver = std::make_unique<Network::NetworkDriver>();
    return EResult::Success;
}

void NetworkSystem::RegisterNetworkHandlers(entt::registry& registry)
{
    m_Driver->GetDispatcher().RegisterHandler(Network::EPacketId::SyncTransform,
        [this, &registry](const Network::PacketHeader& header, Network::PacketReader& reader, const Network::NetworkAddress& sender ) {
            HandleSyncTransform(header, reader, registry);
        });
}

void NetworkSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    if (!m_Driver) return;

    // 패킷 핸들러 지연 바인딩 (레지스트리 참조 연결)
    static bool s_HandlersRegistered = false;
    if (!s_HandlersRegistered)
    {
        RegisterNetworkHandlers(registry);
        s_HandlersRegistered = true;
    }

    // 1. 소켓 수신 폴링, 하트비트, 타임아웃 틱
    m_Driver->Update(dt);

    // 2. 서버/클라이언트 모드별 트랜스폼 동기화 처리
    if (m_Driver->IsServer())
    {
        UpdateServerSync(registry, dt);
    }
    else if (m_Driver->IsClient())
    {
        UpdateClientInterpolation(registry, dt);
    }
}

void NetworkSystem::UpdateServerSync(entt::registry& registry, f32 dt)
{
    auto view = registry.view<NetworkIdentityComponent, TransformComponent, NetworkTransformComponent>();

    for (auto entity : view)
    {
        auto& netId = view.get<NetworkIdentityComponent>(entity);
        auto& trans = view.get<TransformComponent>(entity);
        auto& netTrans = view.get<NetworkTransformComponent>(entity);

        if (netId.NetworkId == Network::InvalidNetId || netId.IsServerOnly) continue;

        netTrans.SyncAccumulator += dt;
        float syncInterval = (netTrans.SyncRate > 0.0f) ? (1.0f / netTrans.SyncRate) : 0.05f;

        if (netTrans.SyncAccumulator >= syncInterval)
        {
            netTrans.SyncAccumulator = 0.0f;

            // 더티 체크 (위치 또는 회전이 임계치 이상 변경되었는가?)
            bool posDirty = netTrans.SyncPosition && (glm::distance(trans.position, netTrans.LastSentPosition) >= netTrans.PositionThreshold);
            bool rotDirty = netTrans.SyncRotation && (glm::length(trans.rotation - netTrans.LastSentRotation) >= netTrans.RotationThreshold);

            if (posDirty || rotDirty || !netTrans.HasInitialState)
            {
                Network::PacketWriter writer(Network::EPacketId::SyncTransform);
                writer.Write<Network::NetId>(netId.NetworkId);

                std::uint8_t flags = (posDirty || !netTrans.HasInitialState ? 1 : 0) | (rotDirty || !netTrans.HasInitialState ? 2 : 0);
                writer.Write<std::uint8_t>(flags);

                writer.WriteVec3(trans.position);
                writer.WriteQuat(trans.rotation);

                m_Driver->Broadcast(writer);

                netTrans.LastSentPosition = trans.position;
                netTrans.LastSentRotation = trans.rotation;
                netTrans.HasInitialState = true;
            }
        }
    }
}

void NetworkSystem::UpdateClientInterpolation(entt::registry& registry, f32 dt)
{
    auto view = registry.view<NetworkIdentityComponent, TransformComponent, NetworkTransformComponent>();
    Network::ClientId localId = m_Driver->GetLocalClientId();

    for (auto entity : view)
    {
        auto& netId = view.get<NetworkIdentityComponent>(entity);
        auto& trans = view.get<TransformComponent>(entity);
        auto& netTrans = view.get<NetworkTransformComponent>(entity);

        // 로컬 플레이어가 직접 조작하는 엔티티는 LERP 보간을 건너뜀
        if (netId.IsLocallyControlled(localId)) continue;
        if (!netTrans.HasInitialState) continue;

        // 원격 엔티티를 서버의 TargetPosition/TargetRotation으로 부드럽게 LERP/SLERP 추종
        float t = std::clamp(netTrans.InterpolationSpeed * dt, 0.0f, 1.0f);
        trans.position = glm::mix(trans.position, netTrans.TargetPosition, t);
        trans.rotation = glm::slerp(trans.rotation, netTrans.TargetRotation, t);
        trans.eulerRotation = glm::degrees(glm::eulerAngles(trans.rotation));
    }
}

void NetworkSystem::HandleSyncTransform(const Network::PacketHeader& header, Network::PacketReader& reader, entt::registry& registry)
{
    Network::NetId netId = Network::InvalidNetId;
    std::uint8_t flags = 0;
    glm::vec3 pos{ 0.0f };
    glm::quat rot = glm::identity<glm::quat>();

    if (!reader.Read<Network::NetId>(netId)) return;
    if (!reader.Read<std::uint8_t>(flags)) return;
    if (!reader.ReadVec3(pos)) return;
    if (!reader.ReadQuat(rot)) return;

    entt::entity entity = FindEntityByNetId(netId);
    if (entity == entt::null || !registry.valid(entity)) return;

    auto* trans = registry.try_get<TransformComponent>(entity);
    auto* netTrans = registry.try_get<NetworkTransformComponent>(entity);
    if (!trans || !netTrans) return;

    if (!netTrans->HasInitialState)
    {
        // 최초 수신 시에는 보간 없이 즉시 스냅(Teleport)
        trans->position = pos;
        trans->rotation = rot;
        trans->eulerRotation = glm::degrees(glm::eulerAngles(rot));

        netTrans->TargetPosition = pos;
        netTrans->TargetRotation = rot;
        netTrans->HasInitialState = true;
    }
    else
    {
        // 이후에는 보간 타깃만 갱신
        netTrans->TargetPosition = pos;
        netTrans->TargetRotation = rot;
    }
}

void NetworkSystem::RegisterEntity(Network::NetId netId, entt::entity entity)
{
    if (netId != Network::InvalidNetId)
    {
        m_NetIdToEntity[netId] = entity;
    }
}

void NetworkSystem::UnregisterEntity(Network::NetId netId)
{
    m_NetIdToEntity.erase(netId);
}

entt::entity NetworkSystem::FindEntityByNetId(Network::NetId netId) const
{
    auto it = m_NetIdToEntity.find(netId);
    return (it != m_NetIdToEntity.end()) ? it->second : entt::null;
}

Network::NetId NetworkSystem::AssignNetId(entt::registry& registry, entt::entity entity, Network::ClientId ownerClientId)
{
    Network::NetId newNetId = m_NextServerNetId++;
    auto& netIdComp = registry.get_or_emplace<NetworkIdentityComponent>(entity);
    netIdComp.NetworkId = newNetId;
    netIdComp.OwnerClientId = ownerClientId;

    RegisterEntity(newNetId, entity);
    return newNetId;
}

END