#include "SocketTransport.h"
#include "PacketStream.h"
#include "PacketDispatcher.h"
#include "NetworkPrediction.h"
#include "NetworkInterpolation.h"
#include <cmath>
#include <iostream>
#include <cassert>

using namespace Engine::Network;

int main()
{
    // 1. 소켓 전송 계층 초기화
    SocketTransport server(ENetworkProtocol::UDP);
    SocketTransport client(ENetworkProtocol::UDP);
    assert(server.Initialize());

    NetworkAddress serverAddr = NetworkAddress::Loopback(9876);
    assert(server.Bind(serverAddr) == ENetworkResult::Success);

    // 2. 디스패처 및 핸들러 등록
    PacketDispatcher dispatcher;
    bool handlerCalled = false;
    std::string receivedMsg;
    glm::vec3 receivedPos;

    dispatcher.RegisterHandler(EPacketId::SyncTransform, [&](<const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender>) {
        handlerCalled = true;
        reader.ReadString(receivedMsg);
        reader.ReadVec3(receivedPos);
        assert(!reader.HasError());
        });

    // 3. 클라이언트에서 패킷 조립 및 UDP 송신
    PacketWriter writer(EPacketId::SyncTransform, 1);
    writer.WriteString("PlayerStateSync");
    writer.WriteVec3(glm::vec3(10.5f, -2.0f, 99.0f));

    std::size_t sendSize = 0;
    const std::uint8_t* sendBuf = writer.FinalizePacket(sendSize);

    std::size_t sentBytes = 0;
    assert(client.SendTo(sendBuf, sendSize, serverAddr, sentBytes) == ENetworkResult::Success);
    assert(sentBytes == sendSize);

    // 4. 서버 수신 및 디스패치
    std::uint8_t recvBuf[512] = { 0 };
    NetworkAddress senderAddr;
    std::size_t recvBytes = 0;

    // 루프백이므로 즉시 수신
    ENetworkResult res = server.ReceiveFrom(recvBuf, sizeof(recvBuf), senderAddr, recvBytes);
    assert(res == ENetworkResult::Success);

    bool dispatched = dispatcher.DispatchPacket(recvBuf, recvBytes, senderAddr);
    assert(dispatched);
    assert(handlerCalled);
    assert(receivedMsg == "PlayerStateSync");
    assert(receivedPos.x == 10.5f && receivedPos.y == -2.0f && receivedPos.z == 99.0f);

    server.Shutdown();

    // 5. NetworkPredictionBuffer: 기록, 확인 및 오차 초과 시 Reconcile 재시뮬레이션
    {
        NetworkPredictionBuffer<float, float> pred(
            64, 0.05f, [](const float& a, const float& b) { return std::fabs(a - b); });

        assert(pred.RecordInput(1, 1.0f, 1.0f));
        assert(pred.RecordInput(2, 1.0f, 2.0f));
        assert(pred.RecordInput(3, 1.0f, 3.0f));
        assert(pred.Acknowledge(1) == 1);

        auto replay = [](float& state, float input) { state += input; };
        const auto staleRes = pred.Reconcile(1, 1.0f, replay);
        assert(staleRes.stale);

        const auto reconRes = pred.Reconcile(2, 2.5f, replay);
        assert(reconRes.corrected);
        assert(reconRes.replayedCount == 1);
        assert(std::fabs(reconRes.state - 3.5f) < 0.001f);
    }

    // 6. NetworkInterpolationBuffer: 지연 보간 및 외삽 상한 방어
    {
        NetworkInterpolationBuffer<NetworkTransformSnapshot> interp(0.1, 0.25, 16);
        NetworkTransformSnapshot s1{};
        s1.position = glm::vec3(0.0f, 0.0f, 0.0f);
        s1.velocity = glm::vec3(10.0f, 0.0f, 0.0f);
        NetworkTransformSnapshot s2{};
        s2.position = glm::vec3(10.0f, 0.0f, 0.0f);
        s2.velocity = glm::vec3(10.0f, 0.0f, 0.0f);

        assert(interp.PushSnapshot(1.0, s1));
        assert(interp.PushSnapshot(2.0, s2));

        const auto mid = interp.Sample(1.6, LerpTransformSnapshot);
        assert(mid.valid && !mid.extrapolated);
        assert(std::fabs(mid.alpha - 0.5f) < 0.001f);
        assert(std::fabs(mid.state.position.x - 5.0f) < 0.001f);

        const auto extrap = interp.Sample(3.0, LerpTransformSnapshot, ExtrapolateTransformSnapshot);
        assert(extrap.valid && extrap.extrapolated);
        assert(std::fabs(extrap.sampleTime - 2.25) < 0.001);
        assert(std::fabs(extrap.state.position.x - 12.5f) < 0.001f);
    }

    std::cout << "[PASS] NetworkProtocolCheck: Packet serialization, transport, dispatch, prediction and interpolation verified successfully!\n";
    return 0;
}