#pragma once
// Minimal stub for the skin-only port: the skin subsystem never touches the
// input struct or user-cmd protobufs, it only needs the type name to hold a
// pointer in c_interfaces. The full nerv version pulled in usercmd.pb.h which
// TempleWare does not expose to this module.

class c_user_cmd;

class i_csgo_input {
public:
	// intentionally opaque — no members/methods are used by the skin changer.
};
