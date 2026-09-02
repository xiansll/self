#include "CInventoryItemsManager.hpp"

#include <ImGui/imgui.h>   // ImColor for GetRarityColor (Andromeda's own ImGui, isolated TU)

#include <Common/Helpers/StringHelper.hpp>

#include <algorithm>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Econ/CEconItem.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>

#include <CS2/Protobuf/base_gcmessages.pb.h>

#include <CS2/SDK/Cstrike15/CCSPlayerInventory.hpp>
#include <CS2/SDK/Cstrike15/CCSInventoryManager.hpp>

#include <CS2/SDK/Econ/CEconItemSchema.hpp>
#include <CS2/SDK/Econ/CEconItemSystem.hpp>
#include <CS2/SDK/Econ/CEconItemDefinition.hpp>

#include <CS2/SDK/GCSDK/GCSDKTypes/EconItemConstants.hpp>

#include <CS2/SDK/Interface/CSource2Client.hpp>
#include <CS2/SDK/Interface/CLocalize.hpp>
#include <CS2/SDK/Interface/IBaseFileSystem.hpp>

#include <GameClient/CL_ItemDefinition.hpp>

#include <AndromedaClient/Features/CInventoryChanger/CInventoryChanger.hpp>
#include <AndromedaClient/Settings/CSettingsJson.hpp>

static CInventoryItemsManager g_CInventoryItemsManager{};

//scan all available items
auto CInventoryItemsManager::ScanAllItems() -> void
{
	auto* pItemSchema = SDK::Interfaces::Source2Client()->GetEconItemSystem()->GetEconItemSchema();
	auto* pPlayerInventory = CCSPlayerInventory::Get();

	if ( !pItemSchema || !pPlayerInventory )
		return;

	static const std::string SkinIconPath = "panorama/images/econ/default_generated/";

	const CUtlMap<int , CEconItemDefinition*>& vecItems = pItemSchema->GetSortedItemDefinitionMap();
	const CUtlMap<int , CPaintKit*>& vecPaintKits = pItemSchema->GetPaintKits();
	const CUtlMap<int , CMusicKit*>& vecMusicKit = pItemSchema->GetMusicKitDefinitions();

	for ( const auto& it : vecItems )
	{
		auto* pItem = it.m_value;

		if ( !pItem )
			continue;

		const bool isWeapon = pItem->IsWeapon();
		const bool isKnife = pItem->IsKnife( true );
		const bool isGlove = pItem->IsGlove( true );
		const bool isAgent = pItem->IsAgent( true );

		if ( !isWeapon && !isKnife && !isGlove && !isAgent )
			continue;

		//some don't have name
		const char* itemBaseName = pItem->m_pszItemBaseName();

		if ( !itemBaseName || itemBaseName[0] == '\0' )
			continue;

		const uint16_t defIdx = pItem->m_nDefIndex();

		DumpedItem_t dumpedItem;

		dumpedItem.m_DefIdx = defIdx;
		dumpedItem.m_Rarity = pItem->m_nItemRarity();
		dumpedItem.m_LoadoutSlot = pItem->LoadoutSlot();

		//for UI
		const char* localizedName = SDK::Interfaces::Localize()->FindSafe( itemBaseName );

		if ( localizedName && localizedName[0] != '\0' && localizedName[0] != '#' )
			dumpedItem.m_DisplayName = localizedName;
		else if ( itemBaseName && itemBaseName[0] != '\0' )
			dumpedItem.m_DisplayName = itemBaseName;
		else
			dumpedItem.m_DisplayName = "Unknown Item";

		if ( isWeapon )
			dumpedItem.m_ItemType = DUMPED_ITEM_TYPE_WEAPON;
		else if ( isKnife )
			dumpedItem.m_ItemType = DUMPED_ITEM_TYPE_KNIFE;
		else if ( isGlove )
			dumpedItem.m_ItemType = DUMPED_ITEM_TYPE_GLOVE;
		else if ( isAgent )
			dumpedItem.m_ItemType = DUMPED_ITEM_TYPE_AGENT;

		if ( strstr( SDK::Interfaces::Localize()->FindSafe( itemBaseName ) , XorStr( "#CSGO_CustomPlayer" ) ) )
			continue;

		if ( isKnife || isGlove || isAgent )
			dumpedItem.m_UnusualItem = true;

		if ( isAgent && strstr( itemBaseName , XorStr( "map_based" ) ) )
			continue;

		if ( isAgent )
		{
			std::string IconPath = "panorama/images/econ/characters/";
			std::string SkinName = itemBaseName + 19;

			IconPath += "customplayer_";
			IconPath += SkinName;
			IconPath += "_png.vtex_c";

			if ( SDK::Interfaces::BaseFileSystem()->FileExists( IconPath.c_str() ) )
			{
				dumpedItem.m_IconPath = IconPath;
				dumpedItem.m_PaintKitName = SkinName;
			}
		}

		for ( const auto& paintIt : vecPaintKits )
		{
			CPaintKit* pPaintKit = paintIt.m_value;

			if ( !pPaintKit || pPaintKit->nID == 0 || pPaintKit->nID == 9001 )
				continue;

			if ( strstr( pPaintKit->sName , XorStr( "doppler_phase2" ) ) ||
				 strstr( pPaintKit->sName , XorStr( "doppler_phase3" ) ) ||
				 strstr( pPaintKit->sName , XorStr( "doppler_phase4" ) ) )
				continue;

			bool hasValidIcon = false;
			std::string IconPath;

			// Weapons
			if ( !hasValidIcon )
			{
				if ( std::string WeaponName = GetWeaponDescFromDefinitionIndex( dumpedItem.m_DefIdx ); WeaponName.size() )
				{
					IconPath = SkinIconPath + WeaponName + "_" + std::string( pPaintKit->sName ) + "_light_png.vtex_c";

					if ( SDK::Interfaces::BaseFileSystem()->FileExists( IconPath.c_str() ) )
						hasValidIcon = true;
				}
			}

			// Gloves
			if ( !hasValidIcon )
			{
				if ( std::string GloveName = GetGloveDescFromDefinitionIndex( dumpedItem.m_DefIdx ); GloveName.size() )
				{
					IconPath = SkinIconPath + GloveName + "_" + std::string( pPaintKit->sName ) + "_light_png.vtex_c";

					if ( SDK::Interfaces::BaseFileSystem()->FileExists( IconPath.c_str() ) )
						hasValidIcon = true;
				}
			}

			// Knifes		
			if ( !hasValidIcon )
			{
				if ( std::string KnifeName = GetKnifeDescFromDefinitionIndex( dumpedItem.m_DefIdx ); KnifeName.size() )
				{
					IconPath = SkinIconPath + KnifeName + "_" + std::string( pPaintKit->sName ) + "_light_png.vtex_c";

					if ( SDK::Interfaces::BaseFileSystem()->FileExists( IconPath.c_str() ) )
						hasValidIcon = true;
				}
			}

			if ( hasValidIcon )
			{
				DumpedSkin_t dumpedSkin;

				dumpedSkin.m_ID = pPaintKit->nID;
				dumpedSkin.m_Rarity = pPaintKit->nRarity;
				dumpedSkin.m_bLegacyModel = pPaintKit->IsUseLegacyModel();
				dumpedSkin.m_PaintKitName = pPaintKit->sName;

				//Get skin name for UI
				if ( pPaintKit->sDescriptionTag && pPaintKit->sDescriptionTag[0] != '\0' )
				{
					const char* skinName = SDK::Interfaces::Localize()->FindSafe( pPaintKit->sDescriptionTag );

					if ( skinName && skinName[0] != '\0' && skinName[0] != '#' )
						dumpedSkin.m_DisplayName = skinName;
					else if ( pPaintKit->sName && pPaintKit->sName[0] != '\0' )
						dumpedSkin.m_DisplayName = pPaintKit->sName;
					else
						dumpedSkin.m_DisplayName = "Unknown Skin";
				}
				else if ( pPaintKit->sName && pPaintKit->sName[0] != '\0' )
				{
					dumpedSkin.m_DisplayName = pPaintKit->sName;
				}
				else
				{
					dumpedSkin.m_DisplayName = "Default";
				}

				dumpedSkin.m_IconPath = IconPath;
				dumpedItem.m_DumpedSkins.emplace_back( dumpedSkin );
			}
		}

		m_vecDumpedItems.emplace_back( dumpedItem );
	}

	m_vecDumpedStickerKits.clear();

	for ( const auto& it : vecItems )
	{
		auto* pItem = it.m_value;

		if ( !pItem )
			continue;

		const char* typeName = pItem->m_pszItemTypeName();

		if ( !typeName || typeName[0] == '\0' )
			continue;

		if ( !strstr( typeName , XorStr( "Sticker" ) ) )
			continue;

		const char* baseName = pItem->m_pszItemBaseName();

		if ( !baseName || baseName[0] == '\0' )
			continue;

		DumpedStickerKit_t kit;
		kit.m_ID     = static_cast<int>( pItem->m_nDefIndex() );
		kit.m_Rarity = pItem->m_nItemRarity();

		const char* localizedName = SDK::Interfaces::Localize()->FindSafe( baseName );

		if ( localizedName && localizedName[0] != '\0' && localizedName[0] != '#' )
			kit.m_DisplayName = localizedName;
		else if ( baseName[0] != '#' )
			kit.m_DisplayName = baseName;
		else
			kit.m_DisplayName = std::string( baseName + 1 );

		m_vecDumpedStickerKits.emplace_back( kit );
	}

	// Music kits
	for ( const auto& it : vecMusicKit )
	{
		CMusicKit* pItem = it.m_value;

		if ( !pItem || pItem->nID == 1 || pItem->nID == 2 )
			continue;

		DumpedItem_t dumpedItem;
		DumpedSkin_t dumpedSkin;

		dumpedItem.m_DefIdx = EInventoryPrefab_t::INVENTORY_PREFAB_MUSIC_KIT;
		dumpedItem.m_ItemType = DUMPED_ITEM_TYPE_MUSIC;
		dumpedItem.m_Rarity = 1;
		dumpedItem.m_LoadoutSlot = LOADOUT_SLOT_MUSICKIT;
		dumpedItem.m_PaintKitName = pItem->sName;

		if ( pItem->sNameLocToken && pItem->sNameLocToken[0] != '\0' )
		{
			const char* musicName = SDK::Interfaces::Localize()->FindSafe( pItem->sNameLocToken );

			if ( musicName && musicName[0] != '\0' && musicName[0] != '#' )
				dumpedItem.m_DisplayName = musicName;
			else if ( pItem->sName && pItem->sName[0] != '\0' )
				dumpedItem.m_DisplayName = pItem->sName;
			else
				dumpedItem.m_DisplayName = "Music Kit";
		}
		else if ( pItem->sName && pItem->sName[0] != '\0' )
		{
			dumpedItem.m_DisplayName = pItem->sName;
		}
		else
		{
			dumpedItem.m_DisplayName = "Music Kit";
		}

		std::string SkinPath = "panorama/images/";
		std::string IconPath = SkinPath + pItem->sInventoryImage;

		IconPath += "_png.vtex_c";

		if ( SDK::Interfaces::BaseFileSystem()->FileExists( IconPath.c_str() ) )
			dumpedSkin.m_IconPath = IconPath;

		dumpedSkin.m_ID = pItem->nID;
		dumpedSkin.m_Rarity = 1;
		dumpedSkin.m_DisplayName = dumpedItem.m_DisplayName;
		dumpedSkin.m_PaintKitName = dumpedItem.m_PaintKitName;
		dumpedItem.m_DumpedSkins.emplace_back( dumpedSkin );

		m_vecDumpedItems.emplace_back( dumpedItem );
	}
}

auto CInventoryItemsManager::OnAddAllItems() -> void
{
	if ( m_vecDumpedItems.empty() )
		ScanAllItems();

	ApplyLoadoutFromConfig();
}

auto CInventoryItemsManager::ForceRescanCatalogue() -> bool
{
	m_vecDumpedItems.clear();
	ScanAllItems();

	DEV_LOG( "[Inventory] ForceRescanCatalogue: %zu items dumped\n" , m_vecDumpedItems.size() );

	return !m_vecDumpedItems.empty();
}

auto CInventoryItemsManager::ApplyLoadoutFromConfig() -> void
{
	std::vector<uint64_t> vecToRemove;
	vecToRemove.reserve( m_AddedItems.size() );

	for ( const auto& Entry : m_AddedItems )
		vecToRemove.emplace_back( Entry.first );

	for ( const auto Id : vecToRemove )
		RemoveItemFromInventoryByID( Id );

	GetInventoryChanger()->RefreshAllSkins();

	for ( const auto& Selection : Settings::Inventory::m_vecSelections )
	{
		uint64_t AddedItemID = 0;

		if ( Selection.m_DefIdx == EInventoryPrefab_t::INVENTORY_PREFAB_MUSIC_KIT )
		{
			AddedItemID = AddMusicToInventory( Selection.m_DefIdx , Selection.m_iPaintKit , false );
		}
		else
		{
			AddedItemID = AddSelectedSkinToInventory(
				Selection.m_DefIdx ,
				Selection.m_iPaintKit ,
				Selection.m_flWear ,
				false ,
				Selection.m_iSeed ,
				Selection.m_iStatTrak
			);
		}

		if ( AddedItemID == 0 )
		{
			DEV_LOG( "[Inventory] ApplyLoadoutFromConfig: failed to add DefIdx=%u PaintKit=%d\n" ,
				Selection.m_DefIdx , Selection.m_iPaintKit );
			continue;
		}

		{
			auto itemIt = m_AddedItems.find( AddedItemID );

			if ( itemIt != m_AddedItems.end() && itemIt->second.m_pEconItem )
			{
				CEconItem* pEcon = itemIt->second.m_pEconItem;

				for ( int s = 0; s < 5; ++s )
				{
					const auto& stk = Selection.m_Stickers[s];

					if ( stk.m_iKitID > 0 )
						pEcon->SetSticker( s , stk.m_iKitID , stk.m_flWear , 1.f , 0.f , 0.f , 0.f );
				}
			}
		}

		if ( Selection.m_bEquipped && Selection.m_iSlot >= 0 )
		{
			GetInventoryChanger()->OnEquipItemInLoadout( Selection.m_iTeam , Selection.m_iSlot , AddedItemID );
		}
	}

	DEV_LOG( "[Inventory] ApplyLoadoutFromConfig: applied %zu selections\n" , Settings::Inventory::m_vecSelections.size() );
}

auto CInventoryItemsManager::FindOrCreateSelection( int iTeam , int iSlot ) -> Settings::Inventory::ItemSelection_t&
{
	for ( auto& Selection : Settings::Inventory::m_vecSelections )
	{
		if ( Selection.m_iTeam == iTeam && Selection.m_iSlot == iSlot )
			return Selection;
	}

	Settings::Inventory::ItemSelection_t NewSelection{};
	NewSelection.m_iTeam = iTeam;
	NewSelection.m_iSlot = iSlot;

	Settings::Inventory::m_vecSelections.emplace_back( NewSelection );

	return Settings::Inventory::m_vecSelections.back();
}

auto CInventoryItemsManager::RemoveSelection( int iTeam , int iSlot ) -> void
{
	auto& vecSelections = Settings::Inventory::m_vecSelections;

	vecSelections.erase(
		std::remove_if( vecSelections.begin() , vecSelections.end() ,
			[iTeam , iSlot]( const Settings::Inventory::ItemSelection_t& Selection )
			{
				return Selection.m_iTeam == iTeam && Selection.m_iSlot == iSlot;
			} ) ,
		vecSelections.end()
	);
}

auto CInventoryItemsManager::GetSelection( int iTeam , int iSlot ) -> Settings::Inventory::ItemSelection_t*
{
	for ( auto& Selection : Settings::Inventory::m_vecSelections )
	{
		if ( Selection.m_iTeam == iTeam && Selection.m_iSlot == iSlot )
			return &Selection;
	}

	return nullptr;
}

auto CInventoryItemsManager::AddSelectedSkinToInventory( uint16_t defIdx , int paintKit , float wear , bool equip , int seed , int statTrak ) -> uint64_t
{
	auto* pCCSPlayerInventory = CCSPlayerInventory::Get();

	if ( !pCCSPlayerInventory )
		return 0;

	DumpedItem_t* pFoundItem = nullptr;
	DumpedSkin_t* pFoundSkin = nullptr;

	for ( auto& item : m_vecDumpedItems )
	{
		if ( item.m_DefIdx == defIdx )
		{
			pFoundItem = &item;

			for ( auto& skin : item.m_DumpedSkins )
			{
				if ( skin.m_ID == paintKit )
				{
					pFoundSkin = &skin;
					break;
				}
			}

			break;
		}
	}

	if ( !pFoundItem )
		return 0;

	CEconItem* pItem = CEconItem::Create();

	if ( !pItem )
		return 0;

	auto highestIDs = pCCSPlayerInventory->GetHighestIDs();

	pItem->m_ulID = highestIDs.first + 1;
	pItem->m_unInventory = highestIDs.second + 1;
	pItem->m_unAccountID = uint32_t( pCCSPlayerInventory->GetOwner().m_id );
	pItem->m_unDefIndex = defIdx;

	if ( pFoundItem->m_UnusualItem )
		pItem->m_nQuality = IQ_UNUSUAL;

	int skinRarity = pFoundSkin ? pFoundSkin->m_Rarity : 0;
	pItem->m_nRarity = std::clamp( pFoundItem->m_Rarity + skinRarity - 1 , 0 , ( skinRarity == 7 ) ? 7 : 6 );

	if ( paintKit )
	{
		pItem->SetPaintKit( static_cast<float>( paintKit ) );
	}

	if ( wear > 0.0f )
	{
		pItem->SetPaintWear( wear );
	}

	if ( seed > 0 )
	{
		pItem->SetPaintSeed( static_cast<float>( seed ) );
	}

	if ( statTrak >= 0 )
	{
		pItem->SetStatTrak( statTrak );
	}

	if ( pCCSPlayerInventory->AddEconItem( pItem ) )
	{
		AddedItem_t Item;

		if( pFoundItem )
		{
			Item.m_DefIdx = defIdx;
			Item.m_PaintKit = paintKit;
			Item.m_Rarity = pItem->m_nRarity;
			Item.m_Wear = wear;
			Item.m_UnusualItem = pFoundItem->m_UnusualItem;
			Item.m_ItemType = pFoundItem->m_ItemType;

			if ( pFoundItem->m_ItemType == EDumpedItemType_t::DUMPED_ITEM_TYPE_AGENT )
			{
				Item.m_bLegacyModel = false;
				Item.m_PaintKitName = pFoundItem->m_PaintKitName;
				Item.m_DisplayName = pFoundItem->m_DisplayName;
			}
			else
			{
				if ( pFoundSkin )
				{
					Item.m_bLegacyModel = pFoundSkin->m_bLegacyModel;
					Item.m_PaintKitName = pFoundSkin->m_PaintKitName;
					Item.m_DisplayName = pFoundSkin->m_DisplayName;
				}
			}

			Item.m_pEconItem = pItem;

			m_AddedItems[pItem->m_ulID] = Item;
		}

		auto* pInventoryManager = CCSInventoryManager::Get();
		
		if ( pInventoryManager && pFoundItem )
		{
			auto* pItemSchema = SDK::Interfaces::Source2Client()->GetEconItemSystem()->GetEconItemSchema();
			
			if ( pItemSchema )
			{
				CEconItemDefinition* pItemDef = nullptr;
				const auto& itemMap = pItemSchema->GetSortedItemDefinitionMap();

				for ( const auto& it : itemMap )
				{
					if ( it.m_value && it.m_value->m_nDefIndex() == defIdx )
					{
						pItemDef = it.m_value;
						break;
					}
				}

				if ( pItemDef )
				{
					int loadoutSlot = pItemDef->LoadoutSlot();

					if ( loadoutSlot >= 0 && equip )
					{
						GetInventoryChanger()->OnEquipItemInLoadout( TEAM_TT , loadoutSlot , pItem->m_ulID );
						GetInventoryChanger()->OnEquipItemInLoadout( TEAM_CT , loadoutSlot , pItem->m_ulID );
					}
				}
			}
		}

		DEV_LOG( "[Inventory] Added skin: DefIdx=%d, PaintKit=%d, ID=%llu\n" , defIdx , paintKit , pItem->m_ulID );

		return pItem->m_ulID;
	}

	return 0;
}

auto CInventoryItemsManager::AddMusicToInventory( uint16_t defIdx , int MusicId , bool equip ) -> uint64_t
{
	if ( auto* pCCSPlayerInventory = CCSPlayerInventory::Get(); pCCSPlayerInventory )
	{
		DumpedItem_t* pFoundItem = nullptr;
		DumpedSkin_t* pFoundSkin = nullptr;

		for ( auto& item : m_vecDumpedItems )
		{
			if ( item.m_DefIdx == defIdx )
			{
				pFoundItem = &item;

				for ( auto& skin : item.m_DumpedSkins )
				{
					if ( skin.m_ID == MusicId )
					{
						pFoundSkin = &skin;
						break;
					}
				}
			}
		}

		if ( !pFoundItem || !pFoundSkin )
			return false;

		CEconItem* pItem = CEconItem::Create();

		if ( pItem )
		{
			auto highestIDs = pCCSPlayerInventory->GetHighestIDs();

			pItem->m_ulID = highestIDs.first + 1;
			pItem->m_unInventory = highestIDs.second + 1;
			pItem->m_unAccountID = uint32_t( pCCSPlayerInventory->GetOwner().m_id );
			pItem->m_unDefIndex = EInventoryPrefab_t::INVENTORY_PREFAB_MUSIC_KIT;

			pItem->SetMusicId( MusicId );

			if ( pCCSPlayerInventory->AddEconItem( pItem ) )
			{
				AddedItem_t Item;
				{
					Item.m_DefIdx = EInventoryPrefab_t::INVENTORY_PREFAB_MUSIC_KIT;
					Item.m_PaintKit = MusicId;
					Item.m_Rarity = 1;
					Item.m_UnusualItem = false;
					Item.m_PaintKitName = pFoundSkin->m_PaintKitName;
					Item.m_DisplayName = pFoundSkin->m_DisplayName;
					Item.m_ItemType = DUMPED_ITEM_TYPE_MUSIC;
					Item.m_pEconItem = pItem;
				}
				
				m_AddedItems[pItem->m_ulID] = Item;

				if ( equip )
				{
					GetInventoryChanger()->OnEquipItemInLoadout( 0 , LOADOUT_SLOT_MUSICKIT , pItem->m_ulID );
				}

				return pItem->m_ulID;
			}
		}
	}

	return 0;
}

auto CInventoryItemsManager::RemoveItemFromInventoryByID( uint64_t ID ) -> bool
{
	if ( IsItemIDAdded( ID ) )
	{
		if ( auto* pInventory = CCSPlayerInventory::Get(); pInventory )
		{
			auto Item = m_AddedItems.find( ID );
			{
				pInventory->RemoveEconItem( Item->second.m_pEconItem );
			}

			m_AddedItems.erase( ID );

			return true;
		}
	}

	return false;
}

auto CInventoryItemsManager::GetSkinName( int paintKitId ) -> std::string
{
	for ( const auto& item : m_vecDumpedItems )
	{
		for ( const auto& skin : item.m_DumpedSkins )
		{
			if ( skin.m_ID == paintKitId )
				return skin.m_DisplayName;
		}
	}

	return "Unknown";
}

auto CInventoryItemsManager::GetWeaponName( uint16_t defIdx ) -> std::string
{
	for ( const auto& item : m_vecDumpedItems )
	{
		if ( item.m_DefIdx == defIdx )
			return item.m_DisplayName;
	}

	return "Unknown";
}

auto CInventoryItemsManager::GetRarityName( int rarity ) -> std::string
{
	switch ( rarity )
	{
		case 0: return "Common";
		case 1: return "Consumer Grade";
		case 2: return "Industrial Grade";
		case 3: return "Mil-Spec";
		case 4: return "Restricted";
		case 5: return "Classified";
		case 6: return "Covert";
		case 7: return "Contraband";
		case 99: return "Unsual";
		default: return "Common";
	}
}

auto CInventoryItemsManager::GetStickerName( int stickerKitId ) -> std::string
{
	for ( const auto& kit : m_vecDumpedStickerKits )
	{
		if ( kit.m_ID == stickerKitId )
			return kit.m_DisplayName;
	}

	return "Unknown Sticker";
}

auto CInventoryItemsManager::GetRarityColor( int rarity ) -> ImU32
{
	switch ( rarity )
	{
		case 0: return ImColor( 77 , 116 , 85 );
		case 1: return ImColor( 176 , 195 , 217 );
		case 2: return ImColor( 94 , 152 , 217 );
		case 3: return ImColor( 75 , 105 , 255 );
		case 4: return ImColor( 136 , 71 , 255 );
		case 5: return ImColor( 211 , 44 , 230 );
		case 6: return ImColor( 235 , 75 , 75 );
		case 7: return ImColor( 207 , 106 , 50 );
		case 99: return ImColor( 255 , 215 , 0 );
		default: return ImColor( 222 , 214 , 204 );
	}
}

auto GetInventoryItemsManager() -> CInventoryItemsManager*
{
	return &g_CInventoryItemsManager;
}
