#pragma once

#include <cstdint>

namespace cstypes {

constexpr float tick_interval = 0.015625f;

[[nodiscard]] constexpr int time_to_ticks(float time) noexcept {
    return static_cast<int>(0.5f + time / tick_interval);
}

[[nodiscard]] constexpr float ticks_to_time(int ticks) noexcept {
    return tick_interval * static_cast<float>(ticks);
}

namespace bone_id {
    constexpr std::uint32_t head = 7;
    constexpr std::uint32_t neck = 6;
    constexpr std::uint32_t spine_1 = 2;
    constexpr std::uint32_t spine_2 = 3;
    constexpr std::uint32_t spine_3 = 4;
    constexpr std::uint32_t spine_4 = 23;
    constexpr std::uint32_t pelvis = 1;

    constexpr std::uint32_t left_clavicle = 8;
    constexpr std::uint32_t left_shoulder = 9;
    constexpr std::uint32_t left_elbow = 10;
    constexpr std::uint32_t left_hand = 11;

    constexpr std::uint32_t right_clavicle = 12;
    constexpr std::uint32_t right_shoulder = 13;
    constexpr std::uint32_t right_elbow = 14;
    constexpr std::uint32_t right_hand = 15;

    constexpr std::uint32_t left_hip = 17;
    constexpr std::uint32_t left_knee = 18;
    constexpr std::uint32_t left_foot = 19;

    constexpr std::uint32_t right_hip = 20;
    constexpr std::uint32_t right_knee = 21;
    constexpr std::uint32_t right_foot = 22;
} // namespace bone_id

namespace hit_group {
    constexpr std::uint32_t generic = 0;
    constexpr std::uint32_t head = 1;
    constexpr std::uint32_t chest = 2;
    constexpr std::uint32_t stomach = 3;
    constexpr std::uint32_t left_arm = 4;
    constexpr std::uint32_t right_arm = 5;
    constexpr std::uint32_t left_leg = 6;
    constexpr std::uint32_t right_leg = 7;
    constexpr std::uint32_t neck = 8;
    constexpr std::uint32_t gear = 10;
} // namespace hit_group

namespace weapon_type {
    constexpr std::uint32_t knife = 0;
    constexpr std::uint32_t pistol = 1;
    constexpr std::uint32_t smg = 2;
    constexpr std::uint32_t rifle = 3;
    constexpr std::uint32_t shotgun = 4;
    constexpr std::uint32_t sniper = 5;
    constexpr std::uint32_t lmg = 6;
    constexpr std::uint32_t c4 = 7;
    constexpr std::uint32_t taser = 8;
    constexpr std::uint32_t grenade = 9;
    constexpr std::uint32_t equipment = 10;
    constexpr std::uint32_t healthshot = 11;
} // namespace weapon_type

namespace command_button {
    constexpr std::uint64_t in_attack = 1ull << 0;
    constexpr std::uint64_t in_jump = 1ull << 1;
    constexpr std::uint64_t in_duck = 1ull << 2;
    constexpr std::uint64_t in_forward = 1ull << 3;
    constexpr std::uint64_t in_back = 1ull << 4;
    constexpr std::uint64_t in_use = 1ull << 5;
    constexpr std::uint64_t in_left = 1ull << 7;
    constexpr std::uint64_t in_right = 1ull << 8;
    constexpr std::uint64_t in_moveleft = 1ull << 9;
    constexpr std::uint64_t in_moveright = 1ull << 10;
    constexpr std::uint64_t in_second_attack = 1ull << 11;
    constexpr std::uint64_t in_reload = 1ull << 13;
    constexpr std::uint64_t in_sprint = 1ull << 16;
    constexpr std::uint64_t in_joyautosprint = 1ull << 17;
    constexpr std::uint64_t in_showscores = 1ull << 33;
    constexpr std::uint64_t in_zoom = 1ull << 34;
    constexpr std::uint64_t in_lookatweapon = 1ull << 35;
} // namespace command_button

namespace move_type {
    constexpr std::uint8_t none = 0;
    constexpr std::uint8_t obsolete = 1;
    constexpr std::uint8_t walk = 2;
    constexpr std::uint8_t fly = 3;
    constexpr std::uint8_t fly_gravity = 4;
    constexpr std::uint8_t vphysics = 5;
    constexpr std::uint8_t push = 6;
    constexpr std::uint8_t noclip = 7;
    constexpr std::uint8_t observer = 8;
    constexpr std::uint8_t ladder = 9;
    constexpr std::uint8_t custom = 10;
    constexpr std::uint8_t last = 11;
} // namespace move_type

namespace entity_flag {
    constexpr std::uint32_t on_ground = 1u << 0;
    constexpr std::uint32_t ducking = 1u << 1;
    constexpr std::uint32_t water_jump = 1u << 3;
    constexpr std::uint32_t on_train = 1u << 4;
    constexpr std::uint32_t in_rain = 1u << 5;
    constexpr std::uint32_t frozen = 1u << 6;
    constexpr std::uint32_t at_controls = 1u << 7;
    constexpr std::uint32_t client = 1u << 8;
    constexpr std::uint32_t fake_client = 1u << 9;
    constexpr std::uint32_t in_water = 1u << 10;
    constexpr std::uint32_t hide_hud_scope = 1u << 11;
} // namespace entity_flag

namespace item_definition_index {
    constexpr std::uint16_t weapon_none = 0;
    constexpr std::uint16_t weapon_deagle = 1;
    constexpr std::uint16_t weapon_dualberettas = 2;
    constexpr std::uint16_t weapon_fiveseven = 3;
    constexpr std::uint16_t weapon_glock = 4;
    constexpr std::uint16_t weapon_ak47 = 7;
    constexpr std::uint16_t weapon_aug = 8;
    constexpr std::uint16_t weapon_awp = 9;
    constexpr std::uint16_t weapon_famas = 10;
    constexpr std::uint16_t weapon_g3sg1 = 11;
    constexpr std::uint16_t weapon_galilar = 13;
    constexpr std::uint16_t weapon_m249 = 14;
    constexpr std::uint16_t weapon_m4a4 = 16;
    constexpr std::uint16_t weapon_mac10 = 17;
    constexpr std::uint16_t weapon_p90 = 19;
    constexpr std::uint16_t weapon_repulsor = 20;
    constexpr std::uint16_t weapon_mp5sd = 23;
    constexpr std::uint16_t weapon_ump45 = 24;
    constexpr std::uint16_t weapon_xm1014 = 25;
    constexpr std::uint16_t weapon_bizon = 26;
    constexpr std::uint16_t weapon_mag7 = 27;
    constexpr std::uint16_t weapon_negev = 28;
    constexpr std::uint16_t weapon_sawedoff = 29;
    constexpr std::uint16_t weapon_tec9 = 30;
    constexpr std::uint16_t weapon_zeus = 31;
    constexpr std::uint16_t weapon_p2000 = 32;
    constexpr std::uint16_t weapon_mp7 = 33;
    constexpr std::uint16_t weapon_mp9 = 34;
    constexpr std::uint16_t weapon_nova = 35;
    constexpr std::uint16_t weapon_p250 = 36;
    constexpr std::uint16_t weapon_shield = 37;
    constexpr std::uint16_t weapon_scar20 = 38;
    constexpr std::uint16_t weapon_sg553 = 39;
    constexpr std::uint16_t weapon_ssg08 = 40;
    constexpr std::uint16_t weapon_knife = 41;
    constexpr std::uint16_t weapon_flashbang = 43;
    constexpr std::uint16_t weapon_hegrenade = 44;
    constexpr std::uint16_t weapon_smokegrenade = 45;
    constexpr std::uint16_t weapon_molotov = 46;
    constexpr std::uint16_t weapon_decoy = 47;
    constexpr std::uint16_t weapon_incgrenade = 48;
    constexpr std::uint16_t weapon_c4 = 49;
    constexpr std::uint16_t weapon_kevlar = 50;
    constexpr std::uint16_t weapon_helm = 51;
    constexpr std::uint16_t weapon_heavyassaultsuit = 52;
    constexpr std::uint16_t weapon_defusekit = 55;
    constexpr std::uint16_t weapon_rescuekit = 56;
    constexpr std::uint16_t weapon_medishot = 57;
    constexpr std::uint16_t weapon_musickit = 58;
    constexpr std::uint16_t weapon_m4a1s = 60;
    constexpr std::uint16_t weapon_usps = 61;
    constexpr std::uint16_t weapon_cz75a = 63;
    constexpr std::uint16_t weapon_revolver = 64;
    constexpr std::uint16_t weapon_tagrenade = 68;
    constexpr std::uint16_t weapon_fists = 69;
    constexpr std::uint16_t weapon_breachcharge = 70;
    constexpr std::uint16_t weapon_tablet = 72;
    constexpr std::uint16_t weapon_axe = 75;
    constexpr std::uint16_t weapon_hammer = 76;
    constexpr std::uint16_t weapon_wrench = 78;

    constexpr std::uint16_t weapon_bayonet = 500;
    constexpr std::uint16_t weapon_classic_knife = 503;
    constexpr std::uint16_t weapon_flip_knife = 505;
    constexpr std::uint16_t weapon_gut_knife = 506;
    constexpr std::uint16_t weapon_karambit = 507;
    constexpr std::uint16_t weapon_m9_bayonet = 508;
    constexpr std::uint16_t weapon_huntsman = 509;
    constexpr std::uint16_t weapon_falchion = 512;
    constexpr std::uint16_t weapon_bowie = 514;
    constexpr std::uint16_t weapon_butterfly = 515;
    constexpr std::uint16_t weapon_shadow_daggers = 516;
    constexpr std::uint16_t weapon_paracord = 517;
    constexpr std::uint16_t weapon_survival = 518;
    constexpr std::uint16_t weapon_ursus = 519;
    constexpr std::uint16_t weapon_navaja = 520;
    constexpr std::uint16_t weapon_nomad = 521;
    constexpr std::uint16_t weapon_stiletto = 522;
    constexpr std::uint16_t weapon_talon = 523;
    constexpr std::uint16_t weapon_skeleton = 525;
} // namespace item_definition_index

struct tick_fraction {
    int tick = 0;
    float frac = 0.0f;

    static tick_fraction from_value(float value) noexcept {
        float integer_part = 0.0f;
        float fractional = std::modff(value, &integer_part);

        if (fractional < 0.0f) {
            fractional += 1.0f;
            if (fractional < 1.0f) {
                integer_part -= 1.0f;
            } else {
                fractional = 0.0f;
            }
        }

        tick_fraction result;
        result.tick = (integer_part >= -2147483600.0f && integer_part < 2147483600.0f)
            ? static_cast<int>(integer_part) : 0;
        result.frac = fractional;

        if (result.frac < 0.0f || result.frac >= 1.0f) {
            result.normalize();
        }
        return result;
    }

    tick_fraction subtract_value(float value) const noexcept {
        return subtract(from_value(value));
    }

    void normalize() noexcept {
        float integer_part = 0.0f;
        float fractional = std::modff(frac, &integer_part);

        if (fractional < 0.0f) {
            fractional += 1.0f;
            if (fractional < 1.0f) {
                integer_part -= 1.0f;
            } else {
                fractional = 0.0f;
            }
        }

        int extra = (integer_part >= -2147483600.0f && integer_part < 2147483600.0f)
            ? static_cast<int>(integer_part) : 0;

        if (fractional < 0.0f || fractional >= 1.0f) {
            tick_fraction inner;
            inner.normalize_raw(fractional, extra);
            tick += inner.tick;
            frac = inner.frac;
        } else {
            tick += extra;
            frac = fractional;
        }
    }

    tick_fraction subtract(const tick_fraction& other) const noexcept {
        tick_fraction result;
        result.frac = frac - other.frac;
        result.tick = tick - other.tick;

        if (result.frac < 0.0f) {
            result.frac += 1.0f;
            if (result.frac < 1.0f) {
                result.tick--;
            } else {
                result.frac = 0.0f;
            }
        }

        if (result.frac < 0.0f || result.frac >= 1.0f) {
            result.normalize();
        }
        return result;
    }

private:
    void normalize_raw(float f, int& extra_tick) noexcept {
        float integer_part = 0.0f;
        float fractional = std::modff(f, &integer_part);

        if (fractional < 0.0f) {
            fractional += 1.0f;
            if (fractional < 1.0f) {
                integer_part -= 1.0f;
            } else {
                fractional = 0.0f;
            }
        }

        extra_tick = (integer_part >= -2147483600.0f && integer_part < 2147483600.0f)
            ? static_cast<int>(integer_part) : 0;
        tick += extra_tick;
        frac = fractional;
    }
};

struct strong_handle {
    const void* binding = nullptr;
};

struct kv3_id {
    const char* format = nullptr;
    std::uintptr_t guid_low = 0;
    std::uintptr_t guid_high = 0;
};

struct event_hash {
    std::uintptr_t hash = 0;
    const char* str = nullptr;
};

} // namespace cstypes