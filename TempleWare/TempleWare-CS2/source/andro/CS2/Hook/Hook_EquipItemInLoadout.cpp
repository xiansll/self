#include "Hook_EquipItemInLoadout.hpp"

#include <AndromedaClient/Features/CInventoryChanger/CInventoryChanger.hpp>

auto Hook_EquipItemInLoadout( CCSInventoryManager* pManager , int iTeam , int iSlot , uint64_t iItemID ) -> void
{
	GetInventoryChanger()->OnEquipItemInLoadout( iTeam , iSlot , iItemID );

	EquipItemInLoadout_o( pManager , iTeam , iSlot , iItemID );
}
