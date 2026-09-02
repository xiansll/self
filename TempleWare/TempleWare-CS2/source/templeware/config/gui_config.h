#pragma once
//
// gui_config - config store for the ACTIVE (gui.cpp / Gui::Render) menu.
//
// The active menu binds every control to the Esp:: config structs
// (Esp::g_config, g_aimbot, g_antiaim, g_movement, g_trigger). The older
// internal_config::ConfigManager serializes a different, unused struct set
// (Config::visual_*), so it does NOT persist what the live menu edits.
//
// These Esp:: structs are all trivially-copyable (only bool / int / float /
// fixed arrays), so we snapshot them as a small versioned binary blob. This
// captures every field automatically - no hand-maintained field list to fall
// out of sync when a struct grows. A magic tag + version + per-struct size
// guard means an incompatible file is rejected cleanly instead of corrupting
// the live state.
//
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "../../esp/esp.h"

namespace gui_config
{
    inline std::filesystem::path Folder()
    {
        char* userProfile = nullptr;
        size_t len = 0;
        errno_t err = _dupenv_s(&userProfile, &len, "USERPROFILE");

        std::filesystem::path f;
        if (err != 0 || userProfile == nullptr || len == 0)
        {
            f = ".templeware";
        }
        else
        {
            f = userProfile;
            free(userProfile);
            f /= ".templeware";
        }
        f /= "configs";

        std::error_code ec;
        std::filesystem::create_directories(f, ec);
        return f;
    }

    inline std::filesystem::path PathFor(const std::string& name)
    {
        return Folder() / (name + ".twcfg");
    }

    inline std::vector<std::string> List()
    {
        std::vector<std::string> out;
        auto f = Folder();
        std::error_code ec;
        if (!std::filesystem::exists(f, ec))
            return out;

        for (const auto& e : std::filesystem::directory_iterator(f, ec))
        {
            if (e.is_regular_file() && e.path().extension() == ".twcfg")
                out.push_back(e.path().stem().string());
        }
        return out;
    }

    // File layout: "TWCF" | uint32 version | [ uint32 size, bytes ] per struct.
    static const char     MAGIC[4] = { 'T', 'W', 'C', 'F' };
    static const uint32_t VERSION = 1;

    template <class T>
    inline void WriteBlock(std::ofstream& o, const T& s)
    {
        uint32_t sz = static_cast<uint32_t>(sizeof(T));
        o.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
        o.write(reinterpret_cast<const char*>(&s), sizeof(T));
    }

    // Reads one block into `s`. Returns false (and leaves `s` untouched) if the
    // recorded size doesn't match the current struct - i.e. the layout changed.
    template <class T>
    inline bool ReadBlock(std::ifstream& i, T& s)
    {
        uint32_t sz = 0;
        i.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        if (!i || sz != static_cast<uint32_t>(sizeof(T)))
            return false;
        i.read(reinterpret_cast<char*>(&s), sizeof(T));
        return static_cast<bool>(i);
    }

    inline bool Save(const std::string& name)
    {
        if (name.empty())
            return false;

        std::ofstream o(PathFor(name), std::ios::binary | std::ios::trunc);
        if (!o.is_open())
            return false;

        o.write(MAGIC, 4);
        o.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
        WriteBlock(o, Esp::g_config);
        WriteBlock(o, Esp::g_aimbot);
        WriteBlock(o, Esp::g_antiaim);
        WriteBlock(o, Esp::g_movement);
        WriteBlock(o, Esp::g_trigger);
        return static_cast<bool>(o);
    }

    inline bool Load(const std::string& name)
    {
        std::ifstream i(PathFor(name), std::ios::binary);
        if (!i.is_open())
            return false;

        char m[4] = {};
        i.read(m, 4);
        if (!i || std::memcmp(m, MAGIC, 4) != 0)
            return false;

        uint32_t ver = 0;
        i.read(reinterpret_cast<char*>(&ver), sizeof(ver));
        if (!i || ver != VERSION)
            return false;

        // Decode into temporaries first; only commit if every block matches, so
        // a truncated or incompatible file never leaves settings half-applied.
        Esp::Config      c  = Esp::g_config;
        Esp::AimbotCfg   a  = Esp::g_aimbot;
        Esp::AntiAimCfg  aa = Esp::g_antiaim;
        Esp::MovementCfg mv = Esp::g_movement;
        Esp::TriggerCfg  tg = Esp::g_trigger;

        if (!ReadBlock(i, c) || !ReadBlock(i, a) || !ReadBlock(i, aa) ||
            !ReadBlock(i, mv) || !ReadBlock(i, tg))
            return false;

        Esp::g_config  = c;
        Esp::g_aimbot  = a;
        Esp::g_antiaim = aa;
        Esp::g_movement = mv;
        Esp::g_trigger = tg;
        return true;
    }

    inline bool Remove(const std::string& name)
    {
        std::error_code ec;
        return std::filesystem::remove(PathFor(name), ec);
    }
}
