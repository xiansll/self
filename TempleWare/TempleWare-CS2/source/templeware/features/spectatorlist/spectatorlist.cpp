#include "spectatorlist.h"
#include "../../../cs2/entity/C_EntitySystem/entity.h"

void SpectatorList::OnRender()
{
	if (!Config::visual_world.spectatorList)
		return;

    if (!I::EngineClient->connected() || !I::EngineClient->in_game())
        return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.13f, 0.7f));  // 70% opacity
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.30f, 0.30f, 0.7f));

    ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(1700, 100), ImGuiCond_FirstUseEver);

    ImGui::Begin("Spectator List",
        nullptr,
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar
    );
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "SPECTATORS:");

    C_CSPlayerPawn* pLocalPawn = g_ctx->local_pawn;

    if (!pLocalPawn || pLocalPawn->m_iHealth() < 1)
    {
        ImGui::End();
        return;
    }

    CCSPlayerController* pLocalController = I::GameEntity->Instance->Get<CCSPlayerController>(pLocalPawn->m_hController().index());

    if (!pLocalController || !pLocalController->m_bPawnIsAlive())
    {
        ImGui::End();
        return;
    }

    CBaseHandle localPawnHandle = pLocalController->m_hPawn();

    auto entities = g_entitySystem->get("CCSPlayerController");

    for (auto& entity : entities)
    {
        if (!entity)
            continue;

        CCSPlayerController* Controller = reinterpret_cast<CCSPlayerController*>(entity);
        if (!Controller) continue;
        if (!Controller->m_hPawn().valid()) continue;
        if (!Controller->m_hObserverPawn().valid()) continue;
        if (Controller->m_bPawnIsAlive()) continue;

        auto ObserverPlayer = I::GameEntity->Instance->Get<C_CSPlayerPawn>(Controller->m_hObserverPawn().index());
        if (!ObserverPlayer) continue;

        auto ObserverServices = ObserverPlayer->m_pObserverServices();
        if (!ObserverServices) continue;

        if (!pLocalController) continue;
        if (!pLocalController->m_hPawn().valid()) continue;
        if (!pLocalController->m_bPawnIsAlive()) continue;

        if (ObserverServices->m_hObserverTarget() == pLocalController->m_hPawn()) {
            ImGui::Text("%s", Controller->m_sSanitizedPlayerName());
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(3);
}
