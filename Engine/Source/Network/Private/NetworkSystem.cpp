#include "NetworkSystem.h"
#include "ComponentEnable.h"
#include "TransformComponent.h"
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
    m_RpcRegistry.Clear();
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
        [this, &registry](const Network::PacketHeader& header, Network::PacketReader& reader, const Network::NetworkAddress& sender) {
            HandleSyncTransform(header, reader, registry);
        });

    m_Driver->GetDispatcher().RegisterHandler(Network::EPacketId::RPCMessage,
        [this](const Network::PacketHeader& header, Network::PacketReader& reader, const Network::NetworkAddress& sender) {
            HandleRpcMessage(header, reader);
        });
}

void NetworkSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    if (!m_Driver) return;

    // 패킷 핸들러 지연 바인딩 (레지스트리 참조 연결)
    RegisterNetworkHandlers(registry);

    // RPC 레지스트리 서버 상태 동기화
    m_RpcRegistry.SetIsServer(m_Driver->IsServer());

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
        if (!IsComponentEnabled<NetworkIdentityComponent, NetworkTransformComponent>(registry, entity)) continue;
        auto& netId = view.get<NetworkIdentityComponent>(entity);
        auto& trans = view.get<TransformComponent>(entity);
        auto& netTrans = view.get<NetworkTransformComponent>(entity);

        if (netId.NetworkId == Network::InvalidNetId || netId.IsServerOnly) continue;

        netTrans.SyncAccumulator += dt;
        float syncInterval = (netTrans.SyncRate > 0.0f) ? (1.0f / netTrans.SyncRate) : 0.05f;
        netTrans.SnapshotInterval = syncInterval;

        if (netTrans.SyncAccumulator >= syncInterval)
        {
            glm::vec3 velocity{ 0.0f };
            if (netTrans.HasInitialState && netTrans.SyncAccumulator > 0.0f)
            {
                velocity = (trans.position - netTrans.LastSentPosition) / netTrans.SyncAccumulator;
            }

            netTrans.SyncAccumulator = 0.0f;

            // 더티 체크 (위치 또는 회전이 임계치 이상 변경되었는가?)
            bool posDirty = netTrans.SyncPosition && (glm::distance(trans.position, netTrans.LastSentPosition) >= netTrans.PositionThreshold);
            bool rotDirty = netTrans.SyncRotation && (glm::length(trans.rotation - netTrans.LastSentRotation) >= netTrans.RotationThreshold);

            if (posDirty || rotDirty || !netTrans.HasInitialState)
            {
                Network::PacketWriter writer(Network::EPacketId::SyncTransform);
                writer.Write<Network::NetId>(netId.NetworkId);

                // flags: bit 0 = pos dirty, bit 1 = rot dirty, bit 2 = has velocity
                std::uint8_t flags = (posDirty || !netTrans.HasInitialState ? 1 : 0) |
                                     (rotDirty || !netTrans.HasInitialState ? 2 : 0) | 4;
                writer.Write<std::uint8_t>(flags);

                writer.WriteVec3(trans.position);
                writer.WriteQuat(trans.rotation);
                writer.WriteVec3(velocity);

                m_Driver->Broadcast(writer);

                netTrans.LastSentPosition = trans.position;
                netTrans.LastSentRotation = trans.rotation;
                netTrans.LastSentVelocity = velocity;
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
        if (!IsComponentEnabled<NetworkIdentityComponent, NetworkTransformComponent>(registry, entity)) continue;
        auto& netId = view.get<NetworkIdentityComponent>(entity);
        auto& trans = view.get<TransformComponent>(entity);
        auto& netTrans = view.get<NetworkTransformComponent>(entity);

        // 로컬 플레이어가 직접 조작하는 엔티티는 LERP 보간을 건너뜀
        if (netId.IsLocallyControlled(localId)) continue;
        if (!netTrans.HasInitialState) continue;

        netTrans.TimeSinceLastSnapshot += dt;

        float syncInterval = (netTrans.SyncRate > 0.0f) ? (1.0f / netTrans.SyncRate) : 0.05f;
        netTrans.SnapshotInterval = syncInterval;

        glm::vec3 desiredTargetPos = netTrans.TargetPosition;

        // Dead Reckoning: 스냅샷 주기보다 지연되었을 때 속도 기반 외삽
        if (netTrans.UseDeadReckoning && netTrans.TimeSinceLastSnapshot > syncInterval)
        {
            float extrapTime = netTrans.TimeSinceLastSnapshot - syncInterval;
            extrapTime = std::min(extrapTime, netTrans.MaxExtrapolationTime);
            desiredTargetPos += netTrans.TargetVelocity * extrapTime;
        }

        // 부드러운 LERP/SLERP 추종
        float t = std::clamp(netTrans.InterpolationSpeed * dt, 0.0f, 1.0f);
        trans.position = glm::mix(trans.position, desiredTargetPos, t);
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
    glm::vec3 vel{ 0.0f };

    if (!reader.Read<Network::NetId>(netId)) return;
    if (!reader.Read<std::uint8_t>(flags)) return;
    if (!reader.ReadVec3(pos)) return;
    if (!reader.ReadQuat(rot)) return;
    if (reader.GetRemainingBytes() >= sizeof(glm::vec3))
    {
        reader.ReadVec3(vel);
    }

    entt::entity entity = FindEntityByNetId(netId);
    if (entity == entt::null || !registry.valid(entity) ||
        !IsComponentEnabled<NetworkIdentityComponent, NetworkTransformComponent>(registry, entity)) return;

    auto* trans = registry.try_get<TransformComponent>(entity);
    auto* netTrans = registry.try_get<NetworkTransformComponent>(entity);
    if (!trans || !netTrans) return;

    if (!netTrans->HasInitialState)
    {
        // 최초 수신 시에는 보간 없이 즉시 스냅(Teleport)
        trans->position = pos;
        trans->rotation = rot;
        trans->eulerRotation = glm::degrees(glm::eulerAngles(rot));

        netTrans->PreviousPosition = pos;
        netTrans->PreviousRotation = rot;
        netTrans->TargetPosition = pos;
        netTrans->TargetRotation = rot;
        netTrans->TargetVelocity = vel;
        netTrans->TimeSinceLastSnapshot = 0.0f;
        netTrans->HasInitialState = true;
    }
    else
    {
        // 텔레포트 임계치 검사: 오차가 너무 크면 즉시 스냅
        if (netTrans->TeleportThreshold > 0.0f && glm::distance(trans->position, pos) > netTrans->TeleportThreshold)
        {
            trans->position = pos;
            trans->rotation = rot;
            trans->eulerRotation = glm::degrees(glm::eulerAngles(rot));

            netTrans->PreviousPosition = pos;
            netTrans->PreviousRotation = rot;
            netTrans->TargetPosition = pos;
            netTrans->TargetRotation = rot;
            netTrans->TargetVelocity = vel;
            netTrans->TimeSinceLastSnapshot = 0.0f;
        }
        else
        {
            // 스냅샷 갱신
            netTrans->PreviousPosition = trans->position;
            netTrans->PreviousRotation = trans->rotation;
            netTrans->TargetPosition = pos;
            netTrans->TargetRotation = rot;
            netTrans->TargetVelocity = vel;
            netTrans->TimeSinceLastSnapshot = 0.0f;
        }
    }
}

void NetworkSystem::HandleRpcMessage(const Network::PacketHeader& header, Network::PacketReader& reader)
{
    Network::RpcMessage msg;
    if (!Network::NetworkRpcRegistry::DecodeRpc(reader, msg)) return;

    if (m_Driver->IsServer())
    {
        if (msg.target == Network::ERpcTarget::Server)
        {
            m_RpcRegistry.DispatchIncoming(msg, true);
        }
        else if (msg.target == Network::ERpcTarget::Multicast)
        {
            // 서버에서 로컬 실행 후 타 클라이언트로 브로드캐스트
            m_RpcRegistry.DispatchIncoming(msg, true);

            Network::PacketWriter forwardWriter(Network::EPacketId::RPCMessage);
            Network::NetworkRpcRegistry::EncodeRpc(msg, forwardWriter);
            m_Driver->Broadcast(forwardWriter, msg.senderClientId);
        }
        else if (msg.target == Network::ERpcTarget::Client)
        {
            // 클라이언트 지정 RPC를 서버가 브로드캐스트/전달
            Network::PacketWriter forwardWriter(Network::EPacketId::RPCMessage);
            Network::NetworkRpcRegistry::EncodeRpc(msg, forwardWriter);
            m_Driver->Broadcast(forwardWriter, msg.senderClientId);
        }
    }
    else if (m_Driver->IsClient())
    {
        m_RpcRegistry.DispatchIncoming(msg, false);
    }
}

bool NetworkSystem::SendRpc(const Network::RpcMessage& message, Network::ClientId targetClientId)
{
    if (!m_Driver) return false;

    // 서버가 ServerRPC를 로컬 호출하는 경우 즉시 디스패치
    if (m_Driver->IsServer() && message.target == Network::ERpcTarget::Server)
    {
        return m_RpcRegistry.DispatchIncoming(message, true);
    }

    return m_Driver->SendRpc(message, targetClientId);
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
