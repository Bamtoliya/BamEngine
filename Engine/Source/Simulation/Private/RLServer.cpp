#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "RLServer.h"
#include "Logger.h"
#include <sstream>
#include <vector>
#include <iomanip>
#include <optional>
#include <glaze/glaze.hpp>

namespace Engine
{
	struct RLCommandRequest
	{
		std::string cmd;
		std::optional<uint64_t> seed;
		std::optional<int64_t> action;
	};
	namespace
	{
		// 소켓 전체 바이트 보장 전송 함수 (Partial Send 방지)
		bool SendAll(SOCKET sock, const std::string& data)
		{
			int totalSent = 0;
			const int toSend = static_cast<int>(data.length());
			while (totalSent < toSend)
			{
				int sent = ::send(sock, data.data() + totalSent, toSend - totalSent, 0);
				if (sent <= 0)
				{
					return false;
				}
				totalSent += sent;
			}
			return true;
		}
		// 개행 없이 과도하게 유입되는 비정상 패킷 방어 상한 (64KB)
		constexpr size_t MaxBufferCapacity = 64 * 1024;
	}

	RLServer::RLServer(std::shared_ptr<IRLEnvironment> environment)
		: m_Environment(std::move(environment))
	{
	}

	RLServer::~RLServer()
	{
		Stop();
	}

	bool RLServer::Start(uint16_t port)
	{
		m_Port = port;

		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "WSAStartup failed");
			return false;
		}

		SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenSock == INVALID_SOCKET)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "socket creation failed: {}", WSAGetLastError());
			WSACleanup();
			return false;
		}

		// 포트 즉시 재사용 옵션 설정
		BOOL opt = TRUE;
		::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

		sockaddr_in serverAddr{};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
		serverAddr.sin_port = htons(m_Port);

		// ::bind로 전역 소켓 함수 명시 (std::bind 충돌 방지)
		if (::bind(listenSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "bind failed on port {}: {}", m_Port, WSAGetLastError());
			::closesocket(listenSock);
			WSACleanup();
			return false;
		}

		if (::listen(listenSock, 1) == SOCKET_ERROR)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "listen failed: {}", WSAGetLastError());
			::closesocket(listenSock);
			WSACleanup();
			return false;
		}

		m_ListenSocket = static_cast<uintptr_t>(listenSock);
		m_IsRunning = true;
		BAM_LOG(Info, "Simulation", "RLServer", "RLServer started on 127.0.0.1:{}", m_Port);
		return true;
	}

	bool RLServer::AcceptClient()
	{
		if (!m_IsRunning || m_ListenSocket == ~0ull) return false;

		BAM_LOG(Info, "Simulation", "RLServer", "Waiting for Python client connection...");
		SOCKET clientSock = ::accept(static_cast<SOCKET>(m_ListenSocket), nullptr, nullptr);
		if (clientSock == INVALID_SOCKET)
		{
			if (m_IsRunning) BAM_LOG(Warn, "Simulation", "RLServer", "accept failed: {}", WSAGetLastError());
			return false;
		}

		// Nagle 알고리즘 비활성화 (낮은 지연시간 요청/응답)
		BOOL nodelay = TRUE;
		::setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

		m_ClientSocket = static_cast<uintptr_t>(clientSock);
		BAM_LOG(Info, "Simulation", "RLServer", "Python client connected successfully.");
		return true;
	}

	void RLServer::ServeClient()
	{
		if (m_ClientSocket == ~0ull) return;

		SOCKET clientSock = static_cast<SOCKET>(m_ClientSocket);
		std::string buffer;
		char recvBuf[1024];

		while (m_IsRunning)
		{
			int bytesRead = ::recv(clientSock, recvBuf, sizeof(recvBuf), 0);
			if (bytesRead <= 0)
			{
				BAM_LOG(Info, "Simulation", "RLServer", "Client disconnected.");
				break;
			}

			// 버퍼 오버플로 방어
			if (buffer.size() + bytesRead > MaxBufferCapacity)
			{
				BAM_LOG(Error, "Simulation", "RLServer", "Client request buffer exceeded capacity ({} bytes). Dropping connection.", MaxBufferCapacity);
				break;
			}

			buffer.append(recvBuf, bytesRead);

			// 개행문자(\n) 기준으로 명령어 라인 추출
			size_t newlinePos = 0;
			while ((newlinePos = buffer.find('\n')) != std::string::npos)
			{
				std::string line = buffer.substr(0, newlinePos);
				buffer.erase(0, newlinePos + 1);

				// 캐리지 리턴('\r') 제거
				if (!line.empty() && line.back() == '\r') line.pop_back();
				if (line.empty()) continue;

				std::string response = ProcessCommand(line) + "\n";
				if (!SendAll(clientSock, response))
				{
					BAM_LOG(Error, "Simulation", "RLServer", "Failed to send complete response to client.");
					break;
				}
			}
		}

		::closesocket(clientSock);
		m_ClientSocket = ~0ull;
	}

	void RLServer::Stop()
	{
		m_IsRunning = false;
		if (m_ClientSocket != ~0ull)
		{
			::closesocket(static_cast<SOCKET>(m_ClientSocket));
			m_ClientSocket = ~0ull;
		}
		if (m_ListenSocket != ~0ull)
		{
			::closesocket(static_cast<SOCKET>(m_ListenSocket));
			m_ListenSocket = ~0ull;
		}
		WSACleanup();
	}

	std::string RLServer::ProcessCommand(const std::string& requestLine)
	{
		if (!m_Environment) return "{\"status\":\"error\",\"message\":\"No environment\"}";

		// Glaze를 통한 안전하고 유연한 JSON 파싱
		RLCommandRequest req;
		auto ec = glz::read_json(req, requestLine);
		if (ec)
		{
			return "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}";
		}

		// 1. get_spaces
		if (req.cmd == "get_spaces")
		{
			return SerializeSpacesResponse();
		}

		// 2. reset
		if (req.cmd == "reset")
		{
			RLObservation obs = m_Environment->Reset(req.seed);
			return SerializeResetResponse(obs);
		}

		// 3. step
		if (req.cmd == "step")
		{
			if (!req.action.has_value())
			{
				return "{\"status\":\"error\",\"message\":\"Missing action for step command\"}";
			}

			RLAction action;
			action.Type = ERLActionType::Discrete;
			action.DiscreteAction = req.action.value();

			RLStepResult result = m_Environment->Step(action);
			return SerializeStepResponse(result);
		}

		// 4. close
		if (req.cmd == "close")
		{
			m_Environment->Close();
			return "{\"status\":\"ok\"}";
		}

		return "{\"status\":\"error\",\"message\":\"Unknown command\"}";
	}

	std::string RLServer::SerializeSpacesResponse() const
	{
		RLSpace obsSpace = m_Environment->GetObservationSpace();
		RLSpace actSpace = m_Environment->GetActionSpace();

		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"action_space\":{\"type\":\"discrete\",\"n\":"
			<< actSpace.Discrete.N << "},\"observation_space\":{\"type\":\"box\",\"shape\":[";
		for (size_t i = 0; i < obsSpace.Box.Shape.size(); ++i)
		{
			if (i > 0) ss << ",";
			ss << obsSpace.Box.Shape[i];
		}
		ss << "]}}";
		return ss.str();
	}

	std::string RLServer::SerializeResetResponse(const RLObservation& obs) const
	{
		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"observation\":[";
		for (size_t i = 0; i < obs.Size(); ++i)
		{
			if (i > 0) ss << ",";
			ss << std::fixed << std::setprecision(6) << obs.Data[i];
		}
		ss << "],\"info\":{}}";
		return ss.str();
	}

	std::string RLServer::SerializeStepResponse(const RLStepResult& result) const
	{
		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"observation\":[";
		for (size_t i = 0; i < result.Observation.Size(); ++i)
		{
			if (i > 0) ss << ",";
			ss << std::fixed << std::setprecision(6) << result.Observation.Data[i];
		}
		ss << "],\"reward\":" << std::fixed << std::setprecision(4) << result.Reward
			<< ",\"terminated\":" << (result.Terminated ? "true" : "false")
			<< ",\"truncated\":" << (result.Truncated ? "true" : "false")
			<< ",\"info\":" << result.InfoJson << "}";
		return ss.str();
	}
}