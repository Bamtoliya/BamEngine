#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "RLServer.h"
#include "VectorizedRLEnvironment.h"
#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <vector>
#include <glaze/glaze.hpp>

namespace Engine
{
	struct RLCommandRequest
	{
		std::string cmd;
		std::optional<uint64_t> seed;
		std::optional<int64_t> action;
		std::optional<std::vector<float>> continuous_action;
		std::optional<std::vector<int64_t>> discrete_actions;
		std::optional<std::vector<std::vector<float>>> actions;
	};

	namespace
	{
		std::string ToUpperAscii(std::string_view text)
		{
			std::string result;
			result.reserve(text.size());
			for (char ch : text)
			{
				result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
			}
			return result;
		}

		void AppendFloatArray(std::ostringstream& ss, const std::vector<float>& values, int precision = 6)
		{
			ss << "[";
			for (std::size_t i = 0; i < values.size(); ++i)
			{
				if (i > 0) ss << ",";
				const float v = std::isfinite(values[i]) ? values[i] : 0.0f;
				ss << std::fixed << std::setprecision(precision) << v;
			}
			ss << "]";
		}

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
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
		constexpr std::size_t MaxBufferCapacity = 64 * 1024;
#endif
	}

	RLServer::RLServer(std::shared_ptr<IRLEnvironment> environment)
		: m_Environment(std::move(environment))
	{
	}

	RLServer::~RLServer()
	{
		Stop();
	}

	void RLServer::SetEnvironment(std::shared_ptr<IRLEnvironment> environment)
	{
		m_Environment = std::move(environment);
	}

	void RLServer::SetVectorizedEnvironment(std::shared_ptr<VectorizedRLEnvironment> vectorizedEnvironment)
	{
		m_VectorizedEnvironment = std::move(vectorizedEnvironment);
	}

	bool RLServer::Start(uint16_t port)
	{
		m_Port = port;

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "WSAStartup failed");
			return false;
		}
		m_WsaInitialized = true;

		SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenSock == INVALID_SOCKET)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "socket creation failed: {}", WSAGetLastError());
			WSACleanup();
			m_WsaInitialized = false;
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
			m_WsaInitialized = false;
			return false;
		}

		if (::listen(listenSock, 1) == SOCKET_ERROR)
		{
			BAM_LOG(Error, "Simulation", "RLServer", "listen failed: {}", WSAGetLastError());
			::closesocket(listenSock);
			WSACleanup();
			m_WsaInitialized = false;
			return false;
		}

		m_ListenSocket = static_cast<uintptr_t>(listenSock);
		m_IsRunning = true;
		BAM_LOG(Info, "Simulation", "RLServer", "RLServer started on 127.0.0.1:{}", m_Port);
		return true;
#else
		return false;
#endif
	}

	bool RLServer::AcceptClient()
	{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
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
#else
		return false;
#endif
	}

	void RLServer::ServeClient()
	{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
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
			if (buffer.size() + static_cast<std::size_t>(bytesRead) > MaxBufferCapacity)
			{
				BAM_LOG(Error, "Simulation", "RLServer", "Client request buffer exceeded capacity ({} bytes). Dropping connection.", MaxBufferCapacity);
				break;
			}

			buffer.append(recvBuf, bytesRead);

			// 개행문자(\n) 기준으로 명령어 라인 추출
			std::size_t newlinePos = 0;
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
#endif
	}

	void RLServer::Stop()
	{
		m_IsRunning = false;
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
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
		if (m_WsaInitialized)
		{
			WSACleanup();
			m_WsaInitialized = false;
		}
#endif
	}

	std::string RLServer::ProcessCommand(std::string_view requestLine)
	{
		RLCommandRequest req;
		auto ec = glz::read_json(req, requestLine);
		if (ec)
		{
			return "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}";
		}

		const std::string cmd = ToUpperAscii(req.cmd);

		// ── 벡터화 환경 명령 (VEC_INFO / VEC_RESET / VEC_STEP / VEC_CLOSE) ──
		if (cmd == "VEC_INFO" || cmd == "VEC_GET_SPACES")
		{
			if (!m_VectorizedEnvironment)
			{
				return "{\"status\":\"error\",\"message\":\"No vectorized environment\"}";
			}
			return SerializeVecInfoResponse();
		}

		if (cmd == "VEC_RESET")
		{
			if (!m_VectorizedEnvironment)
			{
				return "{\"status\":\"error\",\"message\":\"No vectorized environment\"}";
			}
			const auto observations = m_VectorizedEnvironment->ResetAll(req.seed);
			return SerializeVecResetResponse(observations);
		}

		if (cmd == "VEC_STEP")
		{
			if (!m_VectorizedEnvironment)
			{
				return "{\"status\":\"error\",\"message\":\"No vectorized environment\"}";
			}

			const std::size_t expected = m_VectorizedEnvironment->GetNumEnvs();
			if (req.actions.has_value())
			{
				if (req.actions->size() != expected)
				{
					return "{\"status\":\"error\",\"message\":\"Action batch size mismatch\"}";
				}
				const RLVectorStepResult result = m_VectorizedEnvironment->StepAll(req.actions.value());
				return SerializeVecStepResponse(result);
			}
			if (req.discrete_actions.has_value())
			{
				if (req.discrete_actions->size() != expected)
				{
					return "{\"status\":\"error\",\"message\":\"Discrete action batch size mismatch\"}";
				}
				const RLVectorStepResult result = m_VectorizedEnvironment->StepAllDiscrete(req.discrete_actions.value());
				return SerializeVecStepResponse(result);
			}
			return "{\"status\":\"error\",\"message\":\"Missing actions for VEC_STEP command\"}";
		}

		if (cmd == "VEC_CLOSE")
		{
			if (!m_VectorizedEnvironment)
			{
				return "{\"status\":\"error\",\"message\":\"No vectorized environment\"}";
			}
			m_VectorizedEnvironment->CloseAll();
			return "{\"status\":\"ok\"}";
		}

		// ── 단일 환경 명령 (하위 호환: get_spaces / INFO / reset / step / close) ──
		if (!m_Environment)
		{
			return "{\"status\":\"error\",\"message\":\"No environment\"}";
		}

		if (cmd == "GET_SPACES" || cmd == "INFO")
		{
			return SerializeSpacesResponse();
		}

		if (cmd == "RESET")
		{
			const RLObservation obs = m_Environment->Reset(req.seed);
			return SerializeResetResponse(obs);
		}

		if (cmd == "STEP")
		{
			RLAction action;
			if (req.continuous_action.has_value())
			{
				action.Type = ERLActionType::Continuous;
				action.ContinuousActions = req.continuous_action.value();
			}
			else if (req.action.has_value())
			{
				action.Type = ERLActionType::Discrete;
				action.DiscreteAction = req.action.value();
			}
			else
			{
				return "{\"status\":\"error\",\"message\":\"Missing action for step command\"}";
			}

			const RLStepResult result = m_Environment->Step(action);
			return SerializeStepResponse(result);
		}

		if (cmd == "CLOSE")
		{
			m_Environment->Close();
			return "{\"status\":\"ok\"}";
		}

		return "{\"status\":\"error\",\"message\":\"Unknown command\"}";
	}

	std::string RLServer::SerializeSpace(const RLSpace& space) const
	{
		std::ostringstream ss;
		if (space.Type == ERLActionType::Discrete)
		{
			ss << "{\"type\":\"discrete\",\"n\":" << space.Discrete.N << "}";
		}
		else
		{
			ss << "{\"type\":\"box\",\"shape\":[";
			for (std::size_t i = 0; i < space.Box.Shape.size(); ++i)
			{
				if (i > 0) ss << ",";
				ss << space.Box.Shape[i];
			}
			ss << "]}";
		}
		return ss.str();
	}

	std::string RLServer::SerializeSpacesResponse() const
	{
		const RLSpace obsSpace = m_Environment->GetObservationSpace();
		const RLSpace actSpace = m_Environment->GetActionSpace();

		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"name\":\"" << m_Environment->GetName()
			<< "\",\"action_space\":" << SerializeSpace(actSpace)
			<< ",\"observation_space\":" << SerializeSpace(obsSpace) << "}";
		return ss.str();
	}

	std::string RLServer::SerializeResetResponse(const RLObservation& obs) const
	{
		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"observation\":";
		AppendFloatArray(ss, obs.Data);
		ss << ",\"info\":{}}";
		return ss.str();
	}

	std::string RLServer::SerializeStepResponse(const RLStepResult& result) const
	{
		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"observation\":";
		AppendFloatArray(ss, result.Observation.Data);
		const float reward = std::isfinite(result.Reward) ? result.Reward : 0.0f;
		ss << ",\"reward\":" << std::fixed << std::setprecision(4) << reward
			<< ",\"terminated\":" << (result.Terminated ? "true" : "false")
			<< ",\"truncated\":" << (result.Truncated ? "true" : "false")
			<< ",\"info\":" << result.InfoJson << "}";
		return ss.str();
	}

	std::string RLServer::SerializeVecInfoResponse() const
	{
		const RLSpace obsSpace = m_VectorizedEnvironment->GetSingleObservationSpace();
		const RLSpace actSpace = m_VectorizedEnvironment->GetSingleActionSpace();

		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"num_envs\":" << m_VectorizedEnvironment->GetNumEnvs()
			<< ",\"auto_reset\":" << (m_VectorizedEnvironment->IsAutoResetEnabled() ? "true" : "false")
			<< ",\"action_space\":" << SerializeSpace(actSpace)
			<< ",\"observation_space\":" << SerializeSpace(obsSpace) << "}";
		return ss.str();
	}

	std::string RLServer::SerializeVecResetResponse(const std::vector<RLObservation>& observations) const
	{
		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"num_envs\":" << observations.size() << ",\"observations\":[";
		for (std::size_t i = 0; i < observations.size(); ++i)
		{
			if (i > 0) ss << ",";
			AppendFloatArray(ss, observations[i].Data);
		}
		ss << "]}";
		return ss.str();
	}

	std::string RLServer::SerializeVecStepResponse(const RLVectorStepResult& result) const
	{
		const std::size_t count = result.Observations.size();
		std::ostringstream ss;
		ss << "{\"status\":\"ok\",\"num_envs\":" << count << ",\"observations\":[";
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i > 0) ss << ",";
			AppendFloatArray(ss, result.Observations[i].Data);
		}

		ss << "],\"rewards\":[";
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i > 0) ss << ",";
			const float reward = std::isfinite(result.Rewards[i]) ? result.Rewards[i] : 0.0f;
			ss << std::fixed << std::setprecision(4) << reward;
		}

		ss << "],\"terminated\":[";
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i > 0) ss << ",";
			ss << (result.Terminated[i] ? "true" : "false");
		}

		ss << "],\"truncated\":[";
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i > 0) ss << ",";
			ss << (result.Truncated[i] ? "true" : "false");
		}

		ss << "],\"final_observations\":[";
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i > 0) ss << ",";
			if (result.FinalObservations[i].has_value())
			{
				AppendFloatArray(ss, result.FinalObservations[i]->Data);
			}
			else
			{
				ss << "null";
			}
		}

		ss << "],\"infos\":[";
		for (std::size_t i = 0; i < count; ++i)
		{
			if (i > 0) ss << ",";
			ss << (result.InfoJsons[i].empty() ? "{}" : result.InfoJsons[i]);
		}

		ss << "]}";
		return ss.str();
	}
}
