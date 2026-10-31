#pragma once

#include "RLEnvironment.h"
#include <memory>
#include <string>
#include <cstdint>

namespace Engine
{
	class ENGINE_API RLServer
	{
	public:
		RLServer(std::shared_ptr<IRLEnvironment> environment);
		~RLServer();

		// 서버 시작 (지정된 포트에서 리슨)
		bool Start(uint16_t port = 9999);

		// 클라이언트 접속 대기 및 수락 (블로킹)
		bool AcceptClient();

		// 단일 클라이언트와의 요청-응답 루프 실행 (클라이언트 접속 종료 시 반환)
		void ServeClient();

		// 서버 종료 및 리소스 해제
		void Stop();

		bool IsRunning() const { return m_IsRunning; }

	private:
		std::string ProcessCommand(const std::string& requestLine);
		std::string SerializeSpacesResponse() const;
		std::string SerializeResetResponse(const RLObservation& obs) const;
		std::string SerializeStepResponse(const RLStepResult& result) const;

	private:
		std::shared_ptr<IRLEnvironment> m_Environment;
		uintptr_t m_ListenSocket = ~0ull; // INVALID_SOCKET
		uintptr_t m_ClientSocket = ~0ull;
		uint16_t m_Port = 9999;
		bool m_IsRunning = false;
	};
}