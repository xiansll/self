#pragma once
#include <cstdint>
#include "../C_BaseEntity/C_BaseEntity.h"
#include "../../../templeware/utils/schema/schema.h"
#include "../../../templeware/utils/math/utlvector/utlvector.h"
#include "../../../templeware/utils/math/utlstring/utlstring.h"
#include "../../../cs2/entity/handle.h"

class C_EconItemView;
class C_AttributeContainer;
class C_AttributeList;

class C_EconEntity : public C_BaseEntity {
public:
    schema(c_utl_vector<C_AttributeContainer>, m_AttributeManager, "C_EconEntity->m_AttributeManager");
    schema(int, m_nFallbackPaintKit, "C_EconEntity->m_nFallbackPaintKit");
    schema(int, m_nFallbackSeed, "C_EconEntity->m_nFallbackSeed");
    schema(float, m_flFallbackWear, "C_EconEntity->m_flFallbackWear");
    schema(int, m_nFallbackStatTrak, "C_EconEntity->m_nFallbackStatTrak");
    schema(int, m_iEntityLevel, "C_EconEntity->m_iEntityLevel");
    schema(int, m_iAccountID, "C_EconEntity->m_iAccountID");
    schema(uint64_t, m_flOriginalOwnerXuid, "C_EconEntity->m_flOriginalOwnerXuidLow");

    void update_subclass(uint16_t def_index = 0) {
        using fn_t = void(__fastcall*)(void*, uint16_t);
        static auto fn = reinterpret_cast<fn_t>(M::scan_absolute("client.dll", "E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 FF 90 ? ? ? ? C3 CC", 0x1));
        if (def_index != 0) {
            char buf[16];
            sprintf_s(buf, "%d", def_index);
            uint32_t hash = HASH(buf);
            this->m_nSubclassID() = hash;
        }
        if (fn) fn(this, def_index);
    }

    // TODO: Requires distinct pattern - currently uses same pattern as update_subclass (copy/paste bug)
    void update_skin(bool force = true) {
        (void)force;
    }

    // TODO: Requires distinct pattern - currently uses same pattern as update_subclass (copy/paste bug)
    void update_weapon_data() {
    }
};

class C_EconItemView {
public:
    schema(std::uint16_t, m_iItemDefinitionIndex, "C_EconItemView->m_iItemDefinitionIndex");
    schema(int32_t, m_iEntityQuality, "C_EconItemView->m_iEntityQuality");
    schema(uint64_t, m_iItemID, "C_EconItemView->m_iItemID");
    schema(uint32_t, m_iItemIDHigh, "C_EconItemView->m_iItemIDHigh");
    schema(uint32_t, m_iItemIDLow, "C_EconItemView->m_iItemIDLow");
    schema(uint32_t, m_iAccountID, "C_EconItemView->m_iAccountID");
    schema(uint32_t, m_iInventoryPosition, "C_EconItemView->m_iInventoryPosition");
    schema(bool, m_bInitialized, "C_EconItemView->m_bInitialized");
    schema(c_utl_vector<C_AttributeList>, m_AttributeList, "C_EconItemView->m_AttributeList");
    schema(String_t<256>, m_szCustomName, "C_EconItemView->m_szCustomName");
    schema(uint32_t, m_nFallbackPaintKit, "C_EconItemView->m_nFallbackPaintKit");
    schema(uint32_t, m_nFallbackSeed, "C_EconItemView->m_nFallbackSeed");
    schema(float, m_flFallbackWear, "C_EconItemView->m_flFallbackWear");
    schema(int, m_nFallbackStatTrak, "C_EconItemView->m_nFallbackStatTrak");
    schema(uint32_t, m_iEntityLevel, "C_EconItemView->m_iEntityLevel");
    schema(uint64_t, m_flOriginalOwnerXuid, "C_EconItemView->m_flOriginalOwnerXuidLow");

    C_EconItemView* get_next() {
        return *reinterpret_cast<C_EconItemView**>(reinterpret_cast<uintptr_t>(this) + 0x8);
    }

    bool is_initialized() {
        return m_bInitialized();
    }

    uint16_t get_definition_index() {
        return m_iItemDefinitionIndex();
    }
};

class C_AttributeList {
public:
    schema(uintptr_t, m_Attributes, "CAttributeList->m_Attributes");
};

class C_AttributeContainer {
public:
    schema(c_utl_vector<C_EconItemView>, m_Item, "C_AttributeContainer->m_Item");
};