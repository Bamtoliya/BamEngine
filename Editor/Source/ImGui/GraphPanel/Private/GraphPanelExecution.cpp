#include "GraphPanel.h"
#include "GraphMessageLocalization.h"
#include "LocalizationManager.h"
#include "PIEManager.h"
#include "SceneManager.h"
#include "Scene.h"
#include "GraphSystem.h"
#include "Components/Graph/GraphBindingComponent.h"
#include "Components/Core/NameComponent.h"

void GraphPanel::InvalidateExecution()
{
	m_Executor.Clear();
	m_ObservedValues.clear();
	m_TraceNode = 0;
	m_TraceLink = 0;
	m_SelectedTrace = 0;
	m_StepElapsed = 0.0f;
	m_FollowTrace = true;
}

void GraphPanel::BeginExecution()
{
	if (m_Domain != EGraphDomain::Logic) return;
	if (m_Executor.GetState() == EGraphExecutionState::Idle)
	{
		m_LastDiagnostic = m_Executor.Load(m_Logic.Data, m_Registry, &m_ExecutionReflection);
		if (!m_LastDiagnostic.Succeeded()) return;
	}
	m_LastDiagnostic = m_Executor.Begin(m_EntryNode, m_ExecutionDelta,
		static_cast<std::size_t>(std::clamp(m_ExecutionBudget, 1, 100000)));
	m_StepElapsed = 0.0f;
}

void GraphPanel::DrawExecutionToolbar()
{
	if (m_Domain != EGraphDomain::Logic) return;

	// ── 2행 구획 A: 실행 미리보기 (Preview) ──
	ImGui::TextColored(ImVec4(0.35f, 0.78f, 1.0f, 1.0f), "[%s]", LOCAL_CSTR("GraphPanel.ExecutionTrace"));
	ImGui::SameLine();

	const auto state = m_Executor.GetState();
	const bool busy = state == EGraphExecutionState::Running || state == EGraphExecutionState::Paused;
	const auto* entry = m_Logic.Data.FindNode(m_EntryNode);
	if (!entry || (entry->Type != "Event.Start" && entry->Type != "Event.Update"))
	{
		m_EntryNode = 0;
		for (const auto& node : m_Logic.Data.GetNodes())
			if (node.Type == "Event.Start" || node.Type == "Event.Update") { m_EntryNode = node.Id; break; }
		entry = m_Logic.Data.FindNode(m_EntryNode);
	}

	ImGui::BeginDisabled(busy);
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
	const string preview = entry ? fmt::format("{} #{}", entry->Type, entry->Id) : LOCAL("GraphPanel.NoEntry");
	if (ImGui::BeginCombo("##GraphEntry", preview.c_str()))
	{
		for (const auto& node : m_Logic.Data.GetNodes())
		{
			if (node.Type != "Event.Start" && node.Type != "Event.Update") continue;
			const string label = fmt::format("{} #{}", node.Type, node.Id);
			if (ImGui::Selectable(label.c_str(), node.Id == m_EntryNode)) m_EntryNode = node.Id;
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!entry || state == EGraphExecutionState::Running);
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.Run")))
	{
		if (state != EGraphExecutionState::Paused) BeginExecution();
		if (m_LastDiagnostic.Succeeded()) { m_Executor.Resume(); m_FollowTrace = true; }
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(state != EGraphExecutionState::Running);
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.Pause"))) m_Executor.Pause();
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!entry);
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.Step")))
	{
		if (!busy) BeginExecution();
		m_Executor.Pause();
		m_Executor.Step();
		m_FollowTrace = true;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!busy);
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.Stop"))) m_Executor.Stop();
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.ResetInstance")))
	{
		InvalidateExecution();
		m_LastDiagnostic = {};
	}
}

void GraphPanel::UpdateExecution()
{
	if (m_Domain != EGraphDomain::Logic) return;
	if (!std::isfinite(m_StepInterval) || m_StepInterval <= 0.0f) m_StepInterval = 0.15f;
	if (m_Executor.GetState() == EGraphExecutionState::Running)
	{
		m_StepElapsed += ImGui::GetIO().DeltaTime;
		for (int i = 0; i < 4 && m_StepElapsed >= m_StepInterval; ++i)
		{
			m_StepElapsed -= m_StepInterval;
			m_Executor.Tick(1);
		}
	}
	if (m_Executor.GetState() == EGraphExecutionState::Failed) m_LastDiagnostic = m_Executor.GetDiagnostic();
	// A frame-local view of the retained trace, not another runtime value store.
	m_ObservedValues.clear();
	for (const auto& event : m_Executor.GetTrace())
		if (event.Kind == EGraphTraceKind::Value) m_ObservedValues[event.Node].insert_or_assign(event.Pin, event.Value);
	if (!m_FollowTrace) return;
	m_SelectedTrace = 0;
	m_TraceNode = 0;
	m_TraceLink = 0;
	for (auto item = m_Executor.GetTrace().rbegin(); item != m_Executor.GetTrace().rend(); ++item)
	{
		if (!m_TraceNode && item->Kind != EGraphTraceKind::Leave) m_TraceNode = item->Node;
		if (!m_TraceLink && item->Kind == EGraphTraceKind::Link) m_TraceLink = item->Link;
		if (m_TraceNode && m_TraceLink) break;
	}
}

const GraphValue* GraphPanel::FindObservedValue(const GraphPinReference& pin) const
{
	if (m_Domain != EGraphDomain::Logic) return nullptr;
	const auto node = m_ObservedValues.find(pin.Node);
	if (node == m_ObservedValues.end()) return nullptr;
	const auto value = node->second.find(pin.Pin);
	return value == node->second.end() ? nullptr : &value->second;
}

std::string GraphPanel::TraceValueText(const GraphValue& value)
{
	return std::visit([](const auto& item) -> std::string
	{
		using T = std::decay_t<decltype(item)>;
		std::ostringstream text;
		text << std::setprecision(9) << std::boolalpha;
		if constexpr (std::is_same_v<T, std::monostate>) return {};
		else if constexpr (std::is_arithmetic_v<T>) text << item;
		else
		{
			text << '(';
			for (std::size_t i = 0; i < item.size(); ++i) { if (i) text << ", "; text << item[i]; }
			text << ')';
		}
		return text.str();
	}, value);
}

void GraphPanel::DrawExecutionTrace()
{
	if (m_Domain != EGraphDomain::Logic) return;
	ImGui::TextUnformatted(LOCAL_CSTR("GraphPanel.ExecutionTrace"));
	const char* states[]{ "GraphPanel.Idle", "GraphPanel.Paused", "GraphPanel.Running",
		"GraphPanel.Completed", "GraphPanel.Stopped", "GraphPanel.Failed" };
	ImGui::TextUnformatted(LOCAL_CSTR(states[static_cast<int>(m_Executor.GetState())]));
	ImGui::Text("%s: %llu", LOCAL_CSTR("GraphPanel.RemainingBudget"),
		static_cast<unsigned long long>(m_Executor.GetRemainingBudget()));
	const auto result = TraceValueText(m_Executor.GetResult());
	if (!result.empty()) ImGui::Text("%s: %s", LOCAL_CSTR("GraphPanel.Result"), result.c_str());
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::DragFloat(LOCAL_CSTR("GraphPanel.StepInterval"), &m_StepInterval,
		0.01f, 0.01f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	const auto state = m_Executor.GetState();
	ImGui::BeginDisabled(state == EGraphExecutionState::Running || state == EGraphExecutionState::Paused);
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputFloat(LOCAL_CSTR("GraphPanel.EventDelta"), &m_ExecutionDelta);
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::DragInt(LOCAL_CSTR("GraphPanel.TaskBudget"), &m_ExecutionBudget,
		100.0f, 1, 100000, "%d", ImGuiSliderFlags_AlwaysClamp);
	ImGui::EndDisabled();
	ImGui::Checkbox(LOCAL_CSTR("GraphPanel.FollowTrace"), &m_FollowTrace);
	if (ImGui::BeginChild("##GraphTraceList", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 12),
		ImGuiChildFlags_Borders))
	{
		const char* kinds[]{ "GraphPanel.TraceEnter", "GraphPanel.TraceLeave", "GraphPanel.TraceLink",
			"GraphPanel.TraceValue", "GraphPanel.TraceIteration", "GraphPanel.TraceError" };
		for (const auto& event : m_Executor.GetTrace())
		{
			const string label = fmt::format("{} {} #{} {} {} {}###Trace{}", event.Sequence,
				LOCAL(kinds[static_cast<int>(event.Kind)]), event.Node, event.Pin,
				TraceValueText(event.Value), LocalizeGraphMessage(event.Message), event.Sequence);
			if (ImGui::Selectable(label.c_str(), m_SelectedTrace == event.Sequence))
			{
				m_Executor.Pause();
				m_FollowTrace = false;
				m_SelectedTrace = event.Sequence;
				m_TraceNode = event.Node;
				m_TraceLink = event.Link;
				m_Logic.PendingSelection = event.Node;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label.substr(0, label.find("###")).c_str());
		}
		if (m_FollowTrace) ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();
	ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.PreviewScope"));
}

void GraphPanel::DrawExecutionMenu()
{
	if (m_Domain != EGraphDomain::Logic) return;
	if (!ImGui::BeginMenu(LOCAL_LABEL("UI_DEBUG").c_str())) return;
	const auto state = m_Executor.GetState();
	const bool busy = (state == EGraphExecutionState::Running || state == EGraphExecutionState::Paused);
	const auto* entry = m_Logic.Data.FindNode(m_EntryNode);
	if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Run").c_str(), "F5", false, entry != nullptr && state != EGraphExecutionState::Running))
	{
		if (state != EGraphExecutionState::Paused) BeginExecution();
		if (m_LastDiagnostic.Succeeded()) { m_Executor.Resume(); m_FollowTrace = true; }
	}
	if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Pause").c_str(), "F6", false, state == EGraphExecutionState::Running))
		m_Executor.Pause();
	if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Step").c_str(), "F10", false, entry != nullptr))
	{
		if (!busy) BeginExecution();
		m_Executor.Pause();
		m_Executor.Step();
		m_FollowTrace = true;
	}
	if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Stop").c_str(), "Shift+F5", false, busy))
		m_Executor.Stop();
	ImGui::Separator();
	if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.ResetInstance").c_str()))
	{
		InvalidateExecution();
		m_LastDiagnostic = {};
	}
	ImGui::EndMenu();
}
void GraphPanel::DrawDebugToolbar()
{
	if (m_Domain != Engine::EGraphDomain::Logic) return;

	ImGui::SameLine(0.0f, 12.0f);
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine(0.0f, 12.0f);

	// ── 2행 구획 B: PIE 라이브 디버그 관찰 ──
	ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "[PIE]");
	ImGui::SameLine();

	auto* scene = SceneManager::IsCreated() ? SceneManager::Get().GetCurrentScene() : nullptr;
	std::string previewName = LOCAL("GraphPanel.DebugNone");
	if (scene && scene->GetRegistry().valid(m_DebugEntity))
	{
		const auto* nameComp = scene->GetRegistry().try_get<Engine::NameComponent>(m_DebugEntity);
		if (nameComp && !nameComp->name.empty())
			previewName = LOCAL_FMT("GraphPanel.DebugNamedEntity", WStrToStr(nameComp->name), entt::to_integral(m_DebugEntity));
		else
			previewName = LOCAL_FMT("GraphPanel.DebugEntity", entt::to_integral(m_DebugEntity));
	}
	else
	{
		m_DebugEntity = entt::null;
	}

	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15.0f);
	if (ImGui::BeginCombo("##DebugEntity", previewName.c_str()))
	{
		if (ImGui::Selectable(LOCAL_LABEL("EditorCommon.None").c_str(), m_DebugEntity == entt::null))
		{
			m_DebugEntity = entt::null;
			m_LinkPulseTimers.clear();
		}
		if (scene)
		{
			auto& reg = scene->GetRegistry();
			for (auto entity : reg.view<Engine::GraphBindingComponent>())
			{
				const auto& binding = reg.get<Engine::GraphBindingComponent>(entity);
				const auto* nameComp = reg.try_get<Engine::NameComponent>(entity);
				std::string itemLabel;
				if (nameComp && !nameComp->name.empty())
					itemLabel = fmt::format("{} (#{})", WStrToStr(nameComp->name), entt::to_integral(entity));
				else
					itemLabel = LOCAL_FMT("GraphPanel.EntityLabel", entt::to_integral(entity));
				if (!binding.graphFile.empty())
					itemLabel += fmt::format(" [{}]", WStrToStr(std::filesystem::path(binding.graphFile).filename().wstring()));
				const bool selected = (m_DebugEntity == entity);
				ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
				itemLabel += "###DebugEntityItem";
				if (ImGui::Selectable(itemLabel.c_str(), selected))
				{
					m_DebugEntity = entity;
					m_LinkPulseTimers.clear();
					m_LastDebugTraceSequence = 0;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", LOCAL_CSTR("GraphPanel.DebugEntityHint"));
}
void GraphPanel::UpdateLiveDebugging()
{
	const float dt = ImGui::GetIO().DeltaTime;
	// 1. 와이어 펄스 지속 타이머 감소
	for (auto it = m_LinkPulseTimers.begin(); it != m_LinkPulseTimers.end(); )
	{
		it->second -= dt;
		if (it->second <= 0.0f) it = m_LinkPulseTimers.erase(it);
		else ++it;
	}
	if (m_Domain != Engine::EGraphDomain::Logic) return;
	// 게임이 일시정지 상태가 아니라면 히트 노드 초기화
	if (!PIEManager::IsCreated() || !PIEManager::Get().IsPaused())
	{
		m_HitBreakpointNode = 0;
	}
	// 2. PIE 중이고 디버그 엔티티가 선택되어 있으면 실시간 트레이스 동기화
	const bool isPIE = PIEManager::IsCreated() && PIEManager::Get().IsPlaying();
	auto* scene = SceneManager::IsCreated() ? SceneManager::Get().GetCurrentScene() : nullptr;
	if (!isPIE || !scene || !scene->GetRegistry().valid(m_DebugEntity)) return;
	const auto* executor = Engine::GraphSystem::GetDebugExecutor(scene, m_DebugEntity);
	if (!executor) return;
	// 신규 트레이스 이벤트 처리 (신호가 지난 링크에 펄스 부여 및 핀 관측값 갱신)
	const auto& trace = executor->GetTrace();
	for (const auto& event : trace)
	{
		if (event.Sequence <= m_LastDebugTraceSequence) continue;
		m_LastDebugTraceSequence = event.Sequence;
		if (event.Kind == Engine::EGraphTraceKind::Link && event.Link != 0)
		{
			m_LinkPulseTimers[event.Link] = 0.4f; // 0.4초 동안 와이어 맥동
		}
		else if (event.Kind == Engine::EGraphTraceKind::Value)
		{
			m_ObservedValues[event.Node].insert_or_assign(event.Pin, event.Value);
		}
	}
}
