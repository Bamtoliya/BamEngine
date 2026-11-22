#pragma once

#include "Engine_API.h"
#include "RLEnvironment.h"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Engine
{
	class VectorizedRLEnvironment;
	struct RLVectorStepResult;

	class ENGINE_API RLServer
	{
	public:
		explicit RLServer(std::shared_ptr<IRLEnvironment> environment = nullptr);
		~RLServer();

		// 단일 환경 및 벡터화 환경 연결
		void SetEnvironment(std::shared_ptr<IRLEnvironment> environment);
		void SetVectorizedEnvironment(std::shared_ptr<VectorizedRLEnvironment> vectorizedEnvironment);
		std::shared_ptr<IRLEnvironment> GetEnvironment() const { return m_Environment; }
		std::shared_ptr<VectorizedRLEnvironment> GetVectorizedEnvironment() const { return m_VectorizedEnvironment; }

		// 소켓 없이 문자열 프로토콜 명령을 직접 처리 (헤드리스 검증 및 내부 디스패치 공용)
		std::string ProcessCommand(std::string_view requestLine);

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
		std::string SerializeSpace(const RLSpace& space) const;
		std::string SerializeSpacesResponse() const;
		std::string SerializeResetResponse(const RLObservation& obs) const;
		std::string SerializeStepResponse(const RLStepResult& result) const;
		std::string SerializeVecInfoResponse() const;
		std::string SerializeVecResetResponse(const std::vector<RLObservation>& observations) const;
		std::string SerializeVecStepResponse(const RLVectorStepResult& result) const;

	private:
		std::shared_ptr<IRLEnvironment> m_Environment;
		std::shared_ptr<VectorizedRLEnvironment> m_VectorizedEnvironment;
		uintptr_t m_ListenSocket = ~0ull; // INVALID_SOCKET
		uintptr_t m_ClientSocket = ~0ull;
		uint16_t m_Port = 9999;
		bool m_IsRunning = false;
		bool m_WsaInitialized = false;
	};
}
