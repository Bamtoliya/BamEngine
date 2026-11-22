#include "NetworkRpc.h"
#include "NetworkDriver.h"
#include "NetworkSystem.h"
#include "NetworkIdentityComponent.h"
#include "NetworkTransformComponent.h"
#include "TransformComponent.h"
#include "PacketStream.h"
#include "PacketHeader.h"
#include <entt/entity/registry.hpp>
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace Engine;
using namespace Engine::Network;

namespace
{
    void TestRpcSerializationRoundtrip()
    {
        std::cout << "[Step 1] Verifying RpcMessage Serialization Roundtrip...\n";

        RpcMessage original;
        original.netId = 42;
        original.rpcNameHash = HashRpcName("TakeDamage");
        original.target = ERpcTarget::Server;
        original.senderClientId = 3;

        // 페이로드: damage (float 45.5f), weapon (string "RocketLauncher"), hitZone (uint32 2)
        PacketWriter payloadWriter;
        payloadWriter.Write<float>(45.5f);
        payloadWriter.WriteString("RocketLauncher");
        payloadWriter.Write<std::uint32_t>(2);
        original.payload.assign(payloadWriter.GetData(), payloadWriter.GetData() + payloadWriter.GetSize());

        // 패킷 인코딩
        PacketWriter writer(EPacketId::RPCMessage);
        NetworkRpcRegistry::EncodeRpc(original, writer);

        std::size_t totalSize = 0;
        const std::uint8_t* data = writer.FinalizePacket(totalSize);
        assert(totalSize > PacketHeader::HeaderSize);

        // 패킷 헤더 및 페이로드 판독
        PacketReader packetReader(data, totalSize);
        PacketHeader header{};
        assert(packetReader.Read<PacketHeader>(header));
        assert(header.IsValid());
        assert(header.PacketId == static_cast<std::uint16_t>(EPacketId::RPCMessage));

        RpcMessage decoded;
        bool decodeOk = NetworkRpcRegistry::DecodeRpc(packetReader, decoded);
        assert(decodeOk);
        assert(decoded.netId == 42);
        assert(decoded.rpcNameHash == HashRpcName("TakeDamage"));
        assert(decoded.target == ERpcTarget::Server);
        assert(decoded.senderClientId == 3);
        assert(decoded.payload.size() == original.payload.size());

        // 페이로드 역직렬화 내용 검증
        PacketReader payloadReader(decoded.payload.data(), decoded.payload.size());
        float damage = 0.0f;
        std::string weapon;
        std::uint32_t hitZone = 0;
        assert(payloadReader.Read<float>(damage));
        assert(payloadReader.ReadString(weapon));
        assert(payloadReader.Read<std::uint32_t>(hitZone));
        assert(damage == 45.5f);
        assert(weapon == "RocketLauncher");
        assert(hitZone == 2);
        assert(!payloadReader.HasError());

        std::cout << "  -> RpcMessage serialization and payload roundtrip passed!\n";
    }

    void TestRpcDispatchAndAuthorityFiltering()
    {
        std::cout << "[Step 2] Verifying RPC Dispatch & Authority Filtering...\n";

        NetworkRpcRegistry registry;

        int serverCallCount = 0;
        int clientCallCount = 0;
        int multicastCallCount = 0;

        registry.RegisterRpc("ServerAttack", ERpcTarget::Server, [&](const RpcMessage& msg) {
            assert(msg.target == ERpcTarget::Server);
            serverCallCount++;
        });

        registry.RegisterRpc("ClientPlayHurtAnim", ERpcTarget::Client, [&](const RpcMessage& msg) {
            assert(msg.target == ERpcTarget::Client);
            clientCallCount++;
        });

        registry.RegisterRpc("MulticastExplosion", ERpcTarget::Multicast, [&](const RpcMessage& msg) {
            assert(msg.target == ERpcTarget::Multicast);
            multicastCallCount++;
        });

        assert(registry.IsRpcRegistered("ServerAttack"));
        assert(registry.IsRpcRegistered("ClientPlayHurtAnim"));
        assert(registry.IsRpcRegistered("MulticastExplosion"));
        assert(!registry.IsRpcRegistered("UnknownRpc"));

        RpcMessage serverMsg{ 10, HashRpcName("ServerAttack"), ERpcTarget::Server, 1, {} };
        RpcMessage clientMsg{ 10, HashRpcName("ClientPlayHurtAnim"), ERpcTarget::Client, 0, {} };
        RpcMessage multiMsg{ 10, HashRpcName("MulticastExplosion"), ERpcTarget::Multicast, 0, {} };

        // --- 서버 컨텍스트 (isServer = true) 검증 ---
        // 1. Server RPC: 서버 실행 허용
        assert(registry.DispatchIncoming(serverMsg, true) == true);
        assert(serverCallCount == 1);

        // 2. Client RPC: 서버 실행 거부 (클라이언트용 RPC를 서버가 로컬 실행하면 안 됨)
        assert(registry.DispatchIncoming(clientMsg, true) == false);
        assert(clientCallCount == 0);

        // 3. Multicast RPC: 서버 실행 허용
        assert(registry.DispatchIncoming(multiMsg, true) == true);
        assert(multicastCallCount == 1);

        // --- 클라이언트 컨텍스트 (isServer = false) 검증 ---
        // 4. Server RPC: 클라이언트 로컬 실행 거부
        assert(registry.DispatchIncoming(serverMsg, false) == false);
        assert(serverCallCount == 1); // 변화 없음

        // 5. Client RPC: 클라이언트 실행 허용
        assert(registry.DispatchIncoming(clientMsg, false) == true);
        assert(clientCallCount == 1);

        // 6. Multicast RPC: 클라이언트 실행 허용
        assert(registry.DispatchIncoming(multiMsg, false) == true);
        assert(multicastCallCount == 2);

        // --- 타깃 위변조 검사 ---
        RpcMessage spoofedMsg = serverMsg;
        spoofedMsg.target = ERpcTarget::Client; // 해시는 ServerAttack(Server)이나 타깃을 Client로 조작
        assert(registry.DispatchIncoming(spoofedMsg, false) == false);
        assert(registry.DispatchIncoming(spoofedMsg, true) == false);

        // --- 미등록 알 수 없는 RPC 검사 ---
        RpcMessage unknownMsg{ 10, 0x12345678, ERpcTarget::Server, 1, {} };
        assert(registry.DispatchIncoming(unknownMsg, true) == false);

        std::cout << "  -> RPC dispatch authority rules (Server/Client/Multicast) passed!\n";
    }

    void TestSnapshotInterpolationAndDeadReckoning()
    {
        std::cout << "[Step 3] Verifying Snapshot Interpolation & Dead Reckoning...\n";

        entt::registry world;
        auto entity = world.create();

        auto& trans = world.emplace<TransformComponent>(entity);
        trans.position = glm::vec3(0.0f, 0.0f, 0.0f);
        trans.rotation = glm::identity<glm::quat>();

        auto& netId = world.emplace<NetworkIdentityComponent>(entity);
        netId.NetworkId = 100;
        netId.OwnerClientId = 1; // 원격 플레이어 (로컬 클라이언트 ID 2와 다름)

        auto& netTrans = world.emplace<NetworkTransformComponent>(entity);
        netTrans.SyncRate = 20.0f; // 50ms 간격
        netTrans.InterpolationSpeed = 10.0f;
        netTrans.UseDeadReckoning = true;
        netTrans.MaxExtrapolationTime = 0.5f;
        netTrans.TeleportThreshold = 10.0f;

        NetworkSystem* netSys = NetworkSystem::Create();
        assert(netSys != nullptr);
        netSys->RegisterEntity(100, entity);

        // 클라이언트 모드로 세팅 (로컬 클라이언트 ID = 2)
        netSys->GetDriver().SetNetworkMode(ENetworkMode::Client, 2);

        // 1단계: 첫 번째 스냅샷 수신 (최초 상태는 보간 없이 즉시 순간이동)
        {
            PacketWriter writer(EPacketId::SyncTransform);
            writer.Write<Network::NetId>(100);
            writer.Write<std::uint8_t>(7); // pos + rot + vel
            writer.WriteVec3(glm::vec3(10.0f, 0.0f, 0.0f));
            writer.WriteQuat(glm::identity<glm::quat>());
            writer.WriteVec3(glm::vec3(5.0f, 0.0f, 0.0f));

            std::size_t sz = 0;
            const std::uint8_t* p = writer.FinalizePacket(sz);
            netSys->GetDriver().GetDispatcher().DispatchPacket(p, sz, NetworkAddress::Loopback(7777));

            assert(netTrans.HasInitialState == true);
            assert(trans.position.x == 10.0f);
            assert(netTrans.TargetPosition.x == 10.0f);
            assert(netTrans.TargetVelocity.x == 5.0f);
        }

        // 2단계: 두 번째 스냅샷 수신 (위치 11.0f, 속도 10.0f)
        {
            PacketWriter writer(EPacketId::SyncTransform);
            writer.Write<Network::NetId>(100);
            writer.Write<std::uint8_t>(7);
            writer.WriteVec3(glm::vec3(11.0f, 0.0f, 0.0f));
            writer.WriteQuat(glm::identity<glm::quat>());
            writer.WriteVec3(glm::vec3(10.0f, 0.0f, 0.0f));

            std::size_t sz = 0;
            const std::uint8_t* p = writer.FinalizePacket(sz);
            netSys->GetDriver().GetDispatcher().DispatchPacket(p, sz, NetworkAddress::Loopback(7777));

            assert(netTrans.TargetPosition.x == 11.0f);
            assert(netTrans.TargetVelocity.x == 10.0f);
        }

        // 3단계: 일반 보간 틱 진행 (dt = 0.02s < 0.05s)
        vector<Scene*> scenes;
        netSys->OnUpdate(world, scenes, 0.02f);
        // 위치가 10.0f에서 11.0f 방향으로 이동했는지 확인
        assert(trans.position.x > 10.0f && trans.position.x < 11.0f);

        // 4단계: Dead Reckoning 외삽 검증
        // 다음 스냅샷이 누락되어 시간이 syncInterval(0.05s)을 초과한 상황 모의 (총 0.25s 이상 경과)
        for (int i = 0; i < 5; ++i)
        {
            netSys->OnUpdate(world, scenes, 0.05f);
        }
        // 속도(10.0f m/s) 기반으로 외삽되어 11.0f를 넘어 전방으로 추정 이동했음을 검증
        assert(trans.position.x > 11.0f);

        // 5단계: TeleportThreshold 초과 시 즉각 텔레포트 검증 (오차 > 10m)
        {
            PacketWriter writer(EPacketId::SyncTransform);
            writer.Write<Network::NetId>(100);
            writer.Write<std::uint8_t>(7);
            writer.WriteVec3(glm::vec3(500.0f, 0.0f, 0.0f)); // 500m로 순간이동
            writer.WriteQuat(glm::identity<glm::quat>());
            writer.WriteVec3(glm::vec3(0.0f, 0.0f, 0.0f));

            std::size_t sz = 0;
            const std::uint8_t* p = writer.FinalizePacket(sz);
            netSys->GetDriver().GetDispatcher().DispatchPacket(p, sz, NetworkAddress::Loopback(7777));

            assert(trans.position.x == 500.0f);
            assert(netTrans.TargetPosition.x == 500.0f);
        }

        netSys->Free();
        std::cout << "  -> Snapshot interpolation, Dead Reckoning extrapolation and teleport passed!\n";
    }

    void TestNetworkSimulatorDelayAndLoss()
    {
        std::cout << "[Step 4] Verifying Network Simulator Delay Queue & Packet Drop...\n";

        NetworkDriver driver;
        driver.SetNetworkMode(ENetworkMode::Client, 1);

        bool packetDelivered = false;
        std::string deliveredContent;

        driver.GetDispatcher().RegisterHandler(EPacketId::CustomStart, [&](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) {
            packetDelivered = true;
            r.ReadString(deliveredContent);
        });

        PacketWriter writer(EPacketId::CustomStart);
        writer.WriteString("SimulatedPayload");
        std::size_t sz = 0;
        const std::uint8_t* p = writer.FinalizePacket(sz);

        // --- Part A: 인위적 지연 큐 검증 (100ms 지연) ---
        NetworkSimSettings simSettings;
        simSettings.simulatedLatencyMs = 100.0f; // 0.10s
        simSettings.simulatedJitterMs = 0.0f;
        simSettings.simulatedPacketLossPercent = 0.0f;
        driver.SetSimSettings(simSettings);

        bool queued = driver.SimulateReceivePacket(p, sz, NetworkAddress::Loopback(7777));
        assert(queued);
        assert(driver.GetSimulatedIncomingQueueSize() == 1);
        assert(!packetDelivered);

        // 40ms 경과: 아직 전달 안 됨
        driver.Update(0.04f);
        assert(driver.GetSimulatedIncomingQueueSize() == 1);
        assert(!packetDelivered);

        // 추가 40ms 경과 (누적 80ms < 100ms): 아직 전달 안 됨
        driver.Update(0.04f);
        assert(driver.GetSimulatedIncomingQueueSize() == 1);
        assert(!packetDelivered);

        // 추가 30ms 경과 (누적 110ms >= 100ms): 지연 큐에서 전달 완료!
        driver.Update(0.03f);
        assert(driver.GetSimulatedIncomingQueueSize() == 0);
        assert(packetDelivered);
        assert(deliveredContent == "SimulatedPayload");

        // --- Part B: 패킷 손실율 100% 검증 (드롭) ---
        packetDelivered = false;
        deliveredContent.clear();

        simSettings.simulatedLatencyMs = 0.0f;
        simSettings.simulatedPacketLossPercent = 100.0f;
        driver.SetSimSettings(simSettings);

        bool received = driver.SimulateReceivePacket(p, sz, NetworkAddress::Loopback(7777));
        assert(!received); // 드롭됨
        assert(driver.GetSimulatedIncomingQueueSize() == 0);
        assert(!packetDelivered);

        driver.Update(0.1f);
        assert(!packetDelivered);

        // --- Part C: 패킷 손실율 0% 검증 (즉시 수신) ---
        simSettings.simulatedPacketLossPercent = 0.0f;
        driver.SetSimSettings(simSettings);

        received = driver.SimulateReceivePacket(p, sz, NetworkAddress::Loopback(7777));
        assert(received);
        assert(packetDelivered);
        assert(deliveredContent == "SimulatedPayload");

        std::cout << "  -> Network simulator latency queue and packet drop passed!\n";
    }
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "  Running NetworkRpcCheck Headless Suite \n";
    std::cout << "========================================\n";

    TestRpcSerializationRoundtrip();
    TestRpcDispatchAndAuthorityFiltering();
    TestSnapshotInterpolationAndDeadReckoning();
    TestNetworkSimulatorDelayAndLoss();

    std::cout << "\n[PASS] NetworkRpcCheck: All RPC and interpolation/simulation tests passed successfully!\n";
    return 0;
}
