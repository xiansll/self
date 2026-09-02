#include "CSettingsJson.hpp"
#include "DllLauncher.hpp"

#include <filesystem>
#include <fstream>

#include <GameClient/CL_ItemDefinition.hpp>

#include <AndromedaClient/Settings/Settings.hpp>
#include <AndromedaClient/Features/CInventoryChanger/CInventoryItemsManager.hpp>

static CSettingsJson g_CSettingsJson{};

auto CSettingsJson::LoadConfig( const std::string& JsonFile ) -> void
{
	auto ConfigLoadedIndex = 0u;
	const auto ConfigFilePath = GetDllDir() + JsonFile;

	for ( const auto& Config : m_vecConfigList )
	{
		if ( Config == JsonFile )
		{
			m_nConfigLoadedIndex = ConfigLoadedIndex;
			break;
		}

		ConfigLoadedIndex++;
	}

	std::ifstream ConfigFile( ConfigFilePath );

	rapidjson::IStreamWrapper StreamWrapper( ConfigFile );
	rapidjson::Document DocumentConfig;

	DocumentConfig.ParseStream( StreamWrapper );

	if ( !DocumentConfig.HasParseError() && DocumentConfig.HasMember( XorStr( "Settings" ) ) )
	{
		const auto& Root = DocumentConfig[XorStr( "Settings" )];

		// Visual — read into stubs for backwards compat; values are unused.
		if ( Root.HasMember( XorStr( "Visual" ) ) )
		{
			const auto& JsonVisual = Root[XorStr( "Visual" )];
			GetBoolJson( JsonVisual , XorStr( "Active" )        , Settings::Visual::Active );
			GetBoolJson( JsonVisual , XorStr( "Team" )          , Settings::Visual::Team );
			GetBoolJson( JsonVisual , XorStr( "Enemy" )         , Settings::Visual::Enemy );
			GetBoolJson( JsonVisual , XorStr( "OnlyVisible" )   , Settings::Visual::OnlyVisible );
			GetBoolJson( JsonVisual , XorStr( "PlayerBox" )     , Settings::Visual::PlayerBox );
			GetBoolJson( JsonVisual , XorStr( "BoneESP" )       , Settings::Visual::BoneESP );
			GetBoolJson( JsonVisual , XorStr( "BoneESPTeam" )   , Settings::Visual::BoneESPTeam );
			GetBoolJson( JsonVisual , XorStr( "BoneESPEnemy" )  , Settings::Visual::BoneESPEnemy );
			GetBoolJson( JsonVisual , XorStr( "Glow" )          , Settings::Visual::Glow );
			GetBoolJson( JsonVisual , XorStr( "GlowTeam" )      , Settings::Visual::GlowTeam );
			GetBoolJson( JsonVisual , XorStr( "GlowEnemy" )     , Settings::Visual::GlowEnemy );
			GetIntJson ( JsonVisual , XorStr( "PlayerBoxType" ) , Settings::Visual::PlayerBoxType , 0 , 3 );
		}

		// Menu
		if ( Root.HasMember( XorStr( "Menu" ) ) )
		{
			const auto& JsonMenu = Root[XorStr( "Menu" )];
			GetIntJson( JsonMenu , XorStr( "MenuAlpha" ) , Settings::Menu::MenuAlpha , 100 , 255 );
			GetIntJson( JsonMenu , XorStr( "MenuStyle" ) , Settings::Menu::MenuStyle  ,   0 ,   3 );
		}

		// Colors — read into stubs for backwards compat; values are unused.
		if ( Root.HasMember( XorStr( "Colors" ) ) )
		{
			const auto& JsonColors = Root[XorStr( "Colors" )];

			if ( JsonColors.HasMember( XorStr( "Visual" ) ) )
			{
				const auto& JsonColorsVisual = JsonColors[XorStr( "Visual" )];
				GetColorJson( JsonColorsVisual , XorStr( "PlayerBoxTT" )         , &Settings::Colors::Visual::PlayerBoxTT.x );
				GetColorJson( JsonColorsVisual , XorStr( "PlayerBoxTT_Visible" ) , &Settings::Colors::Visual::PlayerBoxTT_Visible.x );
				GetColorJson( JsonColorsVisual , XorStr( "PlayerBoxCT" )         , &Settings::Colors::Visual::PlayerBoxCT.x );
				GetColorJson( JsonColorsVisual , XorStr( "PlayerBoxCT_Visible" ) , &Settings::Colors::Visual::PlayerBoxCT_Visible.x );
				GetColorJson( JsonColorsVisual , XorStr( "BoneESPTT" )           , &Settings::Colors::Visual::BoneESPTT.x );
				GetColorJson( JsonColorsVisual , XorStr( "BoneESPTT_Visible" )   , &Settings::Colors::Visual::BoneESPTT_Visible.x );
				GetColorJson( JsonColorsVisual , XorStr( "BoneESPCT" )           , &Settings::Colors::Visual::BoneESPCT.x );
				GetColorJson( JsonColorsVisual , XorStr( "BoneESPCT_Visible" )   , &Settings::Colors::Visual::BoneESPCT_Visible.x );
				GetColorJson( JsonColorsVisual , XorStr( "GlowTT" )              , &Settings::Colors::Visual::GlowTT.x );
				GetColorJson( JsonColorsVisual , XorStr( "GlowTT_Visible" )      , &Settings::Colors::Visual::GlowTT_Visible.x );
				GetColorJson( JsonColorsVisual , XorStr( "GlowCT" )              , &Settings::Colors::Visual::GlowCT.x );
				GetColorJson( JsonColorsVisual , XorStr( "GlowCT_Visible" )      , &Settings::Colors::Visual::GlowCT_Visible.x );
			}
		}

		// Inventory
		Settings::Inventory::m_vecSelections.clear();

		if ( Root.HasMember( XorStr( "Inventory" ) ) )
		{
			const auto& JsonInventory = Root[XorStr( "Inventory" )];

			GetStringJson( JsonInventory , XorStr( "LastLoadedConfig" ) , Settings::Inventory::m_sLastLoadedConfig );

			if ( JsonInventory.HasMember( XorStr( "Selections" ) ) && JsonInventory[XorStr( "Selections" )].IsArray() )
			{
				const auto& JsonSelections = JsonInventory[XorStr( "Selections" )];

				Settings::Inventory::m_vecSelections.reserve( JsonSelections.Size() );

				for ( rapidjson::SizeType i = 0; i < JsonSelections.Size(); ++i )
				{
					const auto& JsonSelection = JsonSelections[i];

					if ( !JsonSelection.IsObject() )
						continue;

					Settings::Inventory::ItemSelection_t Selection;

					int DefIdxInt = 0;

					GetIntJson  ( JsonSelection , XorStr( "Team" )     , Selection.m_iTeam     , -1     , 4      );
					GetIntJson  ( JsonSelection , XorStr( "Slot" )     , Selection.m_iSlot     , -1     , 65535  );
					GetIntJson  ( JsonSelection , XorStr( "DefIdx" )   , DefIdxInt             ,  0     , 65535  );
					GetIntJson  ( JsonSelection , XorStr( "PaintKit" ) , Selection.m_iPaintKit ,  0     , 65535  );
					GetFloatJson( JsonSelection , XorStr( "Wear" )     , Selection.m_flWear    ,  0.f   , 1.f    );
					GetIntJson  ( JsonSelection , XorStr( "Seed" )     , Selection.m_iSeed     ,  0     , 65535  );
					GetIntJson  ( JsonSelection , XorStr( "StatTrak" ) , Selection.m_iStatTrak , -1     , 999999 );
					GetBoolJson ( JsonSelection , XorStr( "Equipped" ) , Selection.m_bEquipped );

					// Sticker slots 0-4
					for ( int s = 0; s < 5; ++s )
					{
						char keyID[16] , keyWear[16];
						snprintf( keyID   , sizeof( keyID   ) , "Sticker%dID"   , s );
						snprintf( keyWear , sizeof( keyWear ) , "Sticker%dWear" , s );

						GetIntJson  ( JsonSelection , keyID   , Selection.m_Stickers[s].m_iKitID ,  0 , 65535 );
						GetFloatJson( JsonSelection , keyWear , Selection.m_Stickers[s].m_flWear ,  0.f , 1.f  );
					}

					Selection.m_DefIdx = static_cast<uint16_t>( DefIdxInt );

					Settings::Inventory::m_vecSelections.emplace_back( Selection );
				}
			}
		}

		Settings::Inventory::m_sLastLoadedConfig = JsonFile;

		GetInventoryItemsManager()->ApplyLoadoutFromConfig();
	}
	else
	{
		DEV_LOG( "[error] LoadConfig: %s -> %s , %i\n" , ConfigFilePath.c_str() ,
			rapidjson::GetParseError_En( DocumentConfig.GetParseError() ) , DocumentConfig.GetErrorOffset() );
	}

	DocumentConfig.Clear();
	ConfigFile.close();
}

auto CSettingsJson::SaveConfig( const std::string& JsonFile ) -> void
{
	const auto ConfigFilePath = GetDllDir() + JsonFile;

	std::ofstream ConfigFile( ConfigFilePath );

	rapidjson::OStreamWrapper StreamWrapper( ConfigFile );
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> ConfigWriter( StreamWrapper );

	ConfigWriter.SetIndent( '\t' , 1 );
	ConfigWriter.SetFormatOptions( rapidjson::PrettyFormatOptions::kFormatSingleLineArray );
	ConfigWriter.SetMaxDecimalPlaces( 4 );

	ConfigWriter.StartObject();
	{
		ConfigWriter.String( XorStr( "Settings" ) );
		ConfigWriter.StartObject();
		{
			// Menu
			ConfigWriter.String( XorStr( "Menu" ) );
			ConfigWriter.StartObject();
			{
				AddIntJson( ConfigWriter , XorStr( "MenuAlpha" ) , Settings::Menu::MenuAlpha );
				AddIntJson( ConfigWriter , XorStr( "MenuStyle" ) , Settings::Menu::MenuStyle  );
			}
			ConfigWriter.EndObject();

			// Inventory
			ConfigWriter.String( XorStr( "Inventory" ) );
			ConfigWriter.StartObject();
			{
				AddStringJson( ConfigWriter , XorStr( "LastLoadedConfig" ) , Settings::Inventory::m_sLastLoadedConfig );

				ConfigWriter.String( XorStr( "Selections" ) );
				ConfigWriter.StartArray();
				{
					for ( const auto& Selection : Settings::Inventory::m_vecSelections )
					{
						ConfigWriter.StartObject();
						{
							AddIntJson  ( ConfigWriter , XorStr( "Team" )     , Selection.m_iTeam );
							AddIntJson  ( ConfigWriter , XorStr( "Slot" )     , Selection.m_iSlot );
							AddIntJson  ( ConfigWriter , XorStr( "DefIdx" )   , static_cast<int>( Selection.m_DefIdx ) );
							AddIntJson  ( ConfigWriter , XorStr( "PaintKit" ) , Selection.m_iPaintKit );
							AddFloatJson( ConfigWriter , XorStr( "Wear" )     , Selection.m_flWear );
							AddIntJson  ( ConfigWriter , XorStr( "Seed" )     , Selection.m_iSeed );
							AddIntJson  ( ConfigWriter , XorStr( "StatTrak" ) , Selection.m_iStatTrak );
							AddBoolJson ( ConfigWriter , XorStr( "Equipped" ) , Selection.m_bEquipped );

							// Sticker slots 0-4
							for ( int s = 0; s < 5; ++s )
							{
								char keyID[16] , keyWear[16];
								snprintf( keyID   , sizeof( keyID   ) , "Sticker%dID"   , s );
								snprintf( keyWear , sizeof( keyWear ) , "Sticker%dWear" , s );

								AddIntJson  ( ConfigWriter , keyID   , Selection.m_Stickers[s].m_iKitID );
								AddFloatJson( ConfigWriter , keyWear , Selection.m_Stickers[s].m_flWear  );
							}
						}
						ConfigWriter.EndObject();
					}
				}
				ConfigWriter.EndArray();
			}
			ConfigWriter.EndObject();
		}
		ConfigWriter.EndObject();
	}
	ConfigWriter.EndObject();

	ConfigFile.close();

	UpdateConfigList();
}

auto CSettingsJson::DeleteConfig( const std::string& JsonFile ) -> void
{
	const auto ConfigFilePath = GetDllDir() + JsonFile;
	DeleteFileA( ConfigFilePath.c_str() );
}

auto CSettingsJson::UpdateConfigList() -> void
{
	m_vecConfigList.clear();

	for ( const auto& Entry : std::filesystem::directory_iterator( GetDllDir().c_str() ) )
	{
		if ( Entry.is_regular_file() )
		{
			if ( Entry.path().extension().string() == XorStr( ".json" ) )
				m_vecConfigList.emplace_back( Entry.path().filename().string() );
		}
	}
}

auto CSettingsJson::GetIntJson( const rapidjson::Value& JsonValue , const char* Name , int& Output , const int Min , const int Max ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsInt() )
		{
			const auto IntValue = Value.GetInt();

			if ( IntValue < Min )
				Output = Min;
			else if ( IntValue > Max )
				Output = Max;
			else
				Output = IntValue;
		}
	}
}

auto CSettingsJson::GetBoolJson( const rapidjson::Value& JsonValue , const char* Name , bool& Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsBool() )
			Output = Value.GetBool();
	}
}

auto CSettingsJson::GetFloatJson( const rapidjson::Value& JsonValue , const char* Name , float& Output , const float Min , const float Max ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && ( Value.IsFloat() || Value.IsDouble() || Value.IsInt() ) )
			Output = std::clamp( Value.GetFloat() , Min , Max );
	}
}

auto CSettingsJson::GetColorJson( const rapidjson::Value& JsonValue , const char* Name , float* Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsArray() && Value.GetArray().Size() == 4 )
		{
			Output[0] = std::clamp( Value.GetArray()[0].GetFloat() , 0.f , 1.f );
			Output[1] = std::clamp( Value.GetArray()[1].GetFloat() , 0.f , 1.f );
			Output[2] = std::clamp( Value.GetArray()[2].GetFloat() , 0.f , 1.f );
			Output[3] = std::clamp( Value.GetArray()[3].GetFloat() , 0.f , 1.f );
		}
	}
}

auto CSettingsJson::GetStringJson( const rapidjson::Value& JsonValue , const char* Name , std::string& Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( Value.IsString() )
			Output = Value.GetString();
	}
}

auto CSettingsJson::GetBoneSelectedJson( const rapidjson::Value& JsonValue , const char* Name , bool* Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsArray() && Value.GetArray().Size() == 4 )
		{
			Output[0] = Value.GetArray()[0].GetBool();
			Output[1] = Value.GetArray()[1].GetBool();
			Output[2] = Value.GetArray()[2].GetBool();
			Output[3] = Value.GetArray()[3].GetBool();
		}
	}
}

auto CSettingsJson::AddIntJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const int& Output ) -> void
{
	Writer.String( Name );
	Writer.Int( Output );
}

auto CSettingsJson::AddUInt64Json( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const uint64_t& Output ) -> void
{
	Writer.String( Name );
	Writer.Uint64( Output );
}

auto CSettingsJson::AddBoolJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const bool& Output ) -> void
{
	Writer.String( Name );
	Writer.Bool( Output );
}

auto CSettingsJson::AddStringJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const std::string& Output ) -> void
{
	Writer.String( Name );
	Writer.String( Output.c_str() );
}

auto CSettingsJson::AddFloatJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const float& Output ) -> void
{
	Writer.String( Name );
	Writer.Double( static_cast<double>( Output ) );
}

auto CSettingsJson::AddColorJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const float* Output ) -> void
{
	Writer.String( Name );
	Writer.StartArray();
	Writer.Double( static_cast<double>( Output[0] ) );
	Writer.Double( static_cast<double>( Output[1] ) );
	Writer.Double( static_cast<double>( Output[2] ) );
	Writer.Double( static_cast<double>( Output[3] ) );
	Writer.EndArray();
}

auto CSettingsJson::AddBoneSelectedJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const bool* Output ) -> void
{
	Writer.String( Name );
	Writer.StartArray();
	Writer.Bool( Output[0] );
	Writer.Bool( Output[1] );
	Writer.Bool( Output[2] );
	Writer.Bool( Output[3] );
	Writer.EndArray();
}

auto GetSettingsJson() -> CSettingsJson*
{
	return &g_CSettingsJson;
}
