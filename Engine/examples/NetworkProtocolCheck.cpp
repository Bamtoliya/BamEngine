#include "SocketTransport.h"
#include "PacketStream.h"
#include "PacketDispatcher.h"
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
    std::cout << "[PASS] NetworkProtocolCheck: Packet serialization, transport and dispatch verified successfully!\n";
    return 0;
}