#include "C_BaseEntity.h"
#include "../C_CSPlayerPawn/C_CSPlayerPawn.h"

bool C_BaseEntity::is_dormant() const {
    if (auto scene = m_pGameSceneNode()) {
        return scene->IsDormant();
    }
    return true;
}