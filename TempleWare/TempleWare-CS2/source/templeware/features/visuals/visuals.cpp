#include "visuals.h"
#include <algorithm>
#include <iostream>
#include "../../hooks/hooks.h"
#include "../../players/players.h"
#include "../../utils/memory/patternscan/patternscan.h"
#include "../../utils/memory/gaa/gaa.h"
#include "../../../../external/imgui/imgui.h"
#include "../../interfaces/interfaces.h"
#include "../../config/config.h"
#include "../../menu/menu.h"
#include "../../../cs2/entity/C_EntitySystem/entity.h"
using namespace Esp;

LocalPlayerCached cached_local;
std::vector<PlayerCache> cached_players;

void Visuals::init() {
    viewMatrix.viewMatrix = (viewmatrix_t*)M::getAbsoluteAddress(M::patternScan("client", "48 8D 0D ? ? ? ? 48 C1 E0 06"), 3, 0); // ??_7CRenderGameSystem@@6B@ Last Index.  ; char *__fastcall sub_2D49F0(__int64, int)
}

void Esp::cache()
{
    if (!I::EngineClient->valid())
    {
        return;
    }

    cached_players.clear();

    auto entities = g_entitySystem->get("CCSPlayerController");

    PlayerType_t type = none;

    for (auto entity : entities)
    {
        if (!entity)
            continue;

        auto Controller = reinterpret_cast<CCSPlayerController*>(entity);
        if (!Controller || Controller == g_ctx->local_controller)
            continue;

        type = none; int health = 0;
        Vector_t position; Vector_t viewOffset;
        const char* name = "none"; const char* weapon_name = "none";
        bool isScoped; float isFlashed;

        if (!Controller->m_hPawn().valid())
        {
            continue;
        }

        //@handle caching local player 
        if (Controller->IsLocalPlayer()) {
            auto LocalPlayer = I::GameEntity->Instance->Get<C_CSPlayerPawn>(Controller->m_hPawn().index());
            if (!LocalPlayer) {
                cached_local.reset();
                continue;
            }

            cached_local.alive = LocalPlayer->m_iHealth() > 0;
            if (LocalPlayer->m_iHealth() > 0) {
                cached_local.poisition = LocalPlayer->m_vOldOrigin();
                cached_local.health = LocalPlayer->m_iHealth();
                cached_local.handle = LocalPlayer->handle().index();
                cached_local.team = LocalPlayer->m_iTeamNum();
            }
            else {
                cached_local.reset();
            }
        }
        else // @handle only players
        {
            auto Player = I::GameEntity->Instance->Get<C_CSPlayerPawn>(Controller->m_hPawn().index());
            if (!Player)
                continue;

            if (Player->m_iHealth() <= 0)
                continue;

            health = Player->m_iHealth();
            name = Controller->m_sSanitizedPlayerName();
            position = Player->m_vOldOrigin(); viewOffset = Player->m_vecViewOffset();
            isScoped = Player->m_bIsScoped(); isFlashed = Player->m_flFlashDuration();

            CCSWeaponBaseVData* pWeaponData = nullptr;
            if (auto pWeaponServices = Player->m_pWeaponServices()) {
                CBaseHandle hWeapon = pWeaponServices->m_hActiveWeapon();
                if (hWeapon.valid()) {
                    if (auto pWeapon = I::GameEntity->Instance->Get<C_CSWeaponBase>(hWeapon.index())) {
                        pWeaponData = pWeapon->m_pWeaponData();
                    }
                }
            }

            if (pWeaponData) {
                weapon_name = pWeaponData->m_szName();
            }
            else
            {
                weapon_name = "c4";
            }

            const char* weaponPrefix = "weapon_";
            const char* weaponNameStart = strstr(weapon_name, weaponPrefix);

            cached_players.emplace_back(entity, Player, Player->handle(),
                type, health, name,
                weapon_name, position, viewOffset, isScoped, isFlashed, Player->m_iTeamNum());
        }
    }
}

void Visuals::esp() {
    // Only proceed if at least one ESP component is enabled
    if (!Config::visual_esp.esp && !Config::visual_esp.showHealth && !Config::visual_esp.espFill && !Config::visual_esp.showNameTags && !Config::visual_esp.espSkeleton) {
        return; // Exit early if no component is enabled
    }

    if (!I::EngineClient->connected() || !I::EngineClient->in_game())
        return;

    //@better example of getting local pawn
    C_CSPlayerPawn* localPawn = g_ctx->local_pawn;
    if (!localPawn) {
        return;
    }

    if (cached_players.empty())
        return;

    for (const auto& Player : cached_players)
    {

        if (!Player.handle.valid() || Player.health <= 0 || Player.handle.index() == INVALID_EHANDLE_INDEX)
            continue;

        if (Config::visual_esp.teamCheck && (Player.team_num == cached_local.team))
            continue;

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        Vector_t feetPos = Player.position;
        Vector_t headPos = Player.position + Player.viewOffset;

        Vector_t feetScreen, headScreen;
        if (!viewMatrix.WorldToScreen(feetPos, feetScreen) ||
            !viewMatrix.WorldToScreen(headPos, headScreen))
            continue;

        float boxHeight = (feetScreen.y - headScreen.y) * 1.3f;
        float boxWidth = boxHeight / 2.0f;

        float centerX = (feetScreen.x + headScreen.x) / 2.0f;
        float boxX = centerX - (boxWidth / 2.0f);

        float boxY = headScreen.y - (boxHeight - (feetScreen.y - headScreen.y)) / 2.0f;

        ImVec4 espColorWithAlpha = Config::visual_esp.espColor;
        espColorWithAlpha.w = Config::visual_esp.espFillOpacity;
        ImU32 boxColor = ImGui::ColorConvertFloat4ToU32(Config::visual_esp.espColor);
        ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(espColorWithAlpha);

        // Skeleton ESP
        if (Config::visual_esp.espSkeleton)
        {
            auto getScreenPos = [&](int id, ImVec2& out) -> bool {
                Vector_t screen;

                auto bonePos = GetBonePos(Player.player, (BoneID)id);

                if (!(bonePos.IsZero()) && viewMatrix.WorldToScreen(bonePos, screen) && !(screen.IsZero())) {
                    out = ImVec2(screen.x, screen.y);
                    return true;
                }
                return false;
                };

            auto drawBone = [&](int a, int b) {
                ImVec2 p1, p2;

                ImU32 color = ImGui::ColorConvertFloat4ToU32(Config::visual_esp.skeletonColor);

                if (!getScreenPos(a, p1) || !getScreenPos(b, p2)) return;
                if (sqrtf(powf(p1.x - p2.x, 2) + powf(p1.y - p2.y, 2)) > 500.f) return;

                drawList->AddLine(p1, p2, color, Config::visual_esp.skeletonThickness);

                };

            drawBone(7, 6);
            drawBone(6, 4);
            drawBone(4, 2);
            drawBone(2, 1);

            drawBone(6, 9);
            drawBone(9, 10);
            drawBone(10, 11);

            drawBone(6, 13);
            drawBone(13, 14);
            drawBone(14, 15);

            drawBone(1, 17);
            drawBone(17, 18);
            drawBone(18, 19);

            drawBone(1, 20);
            drawBone(20, 21);
            drawBone(21, 22);
        }

        // ESP Fill
        if (Config::visual_esp.espFill) {
            drawList->AddRectFilled(
                ImVec2(boxX, boxY),
                ImVec2(boxX + boxWidth, boxY + boxHeight),
                fillColor
            );
        }

        // ESP Box - only render if Config::visual_esp.esp is enabled
        if (Config::visual_esp.esp) {
            drawList->AddRect(
                ImVec2(boxX, boxY),
                ImVec2(boxX + boxWidth, boxY + boxHeight),
                boxColor,
                0.0f,
                0,
                Config::visual_esp.espThickness
            );
        }

        // Using scope
        if (Config::visual_esp.showScoped && Player.IsScoped)
        {
            ImVec2 textSize = ImGui::CalcTextSize("SCOPE");

            float textX = boxX + boxWidth + 6;
            float textY = boxY + (boxHeight - textSize.y) / 2;

            if (Config::visual_esp.showFlashed && Player.IsFlashed != 0.0f)
                textY -= textSize.y / 2 + 2;

            drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), "SCOPE");
        }

        // Flashed
        if (Config::visual_esp.showFlashed && Player.IsFlashed != 0.0f)
        {
            ImVec2 textSize = ImGui::CalcTextSize("BLIND");

            float textX = boxX + boxWidth + 6;
            float textY = boxY + (boxHeight - textSize.y) / 2;

            if (Config::visual_esp.showScoped && Player.IsScoped)
                textY += textSize.y / 2 + 2;

            drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), "BLIND");
        }

        // Health Bar
        if (Config::visual_esp.showHealth) {
            int health = Player.health;
            float healthHeight = boxHeight * (static_cast<float>(health) / 100.0f);
            float barWidth = 4.0f;
            float barX = boxX - (barWidth + 2);
            float barY = boxY + (boxHeight - healthHeight);

            drawList->AddRectFilled(
                ImVec2(barX, boxY),
                ImVec2(barX + barWidth, boxY + boxHeight),
                IM_COL32(70, 70, 70, 255)
            );

            ImU32 healthColor = IM_COL32(
                static_cast<int>((100 - health) * 2.55f),
                static_cast<int>(health * 2.55f),
                0,
                255
            );
            drawList->AddRectFilled(
                ImVec2(barX, barY),
                ImVec2(barX + barWidth, barY + healthHeight),
                healthColor
            );

            std::string displayText = "[" + std::to_string(health) + "HP]";
            ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
            float textX = boxX + (boxWidth - textSize.x) / 2;
            float textY = boxY + boxHeight + 2;
            drawList->AddText(
                ImVec2(textX + 1, textY + 1),
                IM_COL32(0, 0, 0, 255),
                displayText.c_str()
            );
            drawList->AddText(
                ImVec2(textX, textY),
                IM_COL32(255, 255, 255, 255),
                displayText.c_str()
            );
        }

        if (Config::visual_esp.showNameTags) {
            std::string playerName = Player.name;
            ImVec2 nameSize = ImGui::CalcTextSize(playerName.c_str());

            float nameX = boxX + (boxWidth - nameSize.x) / 2;
            float nameY = boxY - nameSize.y - 2;

            //@FIXME: shit method to do outline
            drawList->AddText(
                ImVec2(nameX + 1, nameY + 1),
                IM_COL32(0, 0, 0, 255),
                playerName.c_str()
            );

            drawList->AddText(
                ImVec2(nameX, nameY),
                IM_COL32(255, 255, 255, 255),
                playerName.c_str()
            );
        }
    }
}
