import json
import os

def resolve_field(class_name, field_name, classes):
    current = class_name
    visited = set()
    while current and current not in visited:
        visited.add(current)
        if current in classes:
            cls = classes[current]
            if field_name in cls.get("fields", {}):
                return cls["fields"][field_name]
            parent = cls.get("parent")
            if parent:
                current = parent
            else:
                break
        else:
            break
    return None

file_path = "client_dll.json"
if not os.path.exists(file_path):
    print(f"{file_path} bulunamadı!")
    input()
    exit()

with open(file_path, "r", encoding="utf-8") as f:
    data = json.load(f)

# Doğru yol: data["client.dll"]["classes"]
module_data = data.get("client.dll")
if not module_data:
    print("'client.dll' anahtarı bulunamadı!")
    input()
    exit()

classes = module_data.get("classes", {})
print(f"Toplam sınıf sayısı: {len(classes)}")

target_classes = [
    "CCSPlayerController",
    "CBasePlayerController",
    "C_CSPlayerPawn",
    "CCSPlayerPawn",
    "CCSPlayerPawnBase",
    "C_BaseEntity",
    "CBaseEntity",
    "CEntityInstance",
    "CGameEntitySystem"
]

target_fields = [
    "m_hPlayerPawn",
    "m_sSanitizedPlayerName",
    "m_iHealth",
    "m_iTeamNum",
    "m_vOldOrigin",
    "m_pGameSceneNode",
    "m_pClientSceneNode",
    "m_entityList",
    "m_highestEntityIndex"
]

print()
for class_name in target_classes:
    if class_name not in classes:
        continue
    cls = classes[class_name]
    print(f"Sınıf: {class_name}")
    parent = cls.get("parent")
    if parent:
        print(f"  Parent: {parent}")
    for field in target_fields:
        offset = resolve_field(class_name, field, classes)
        if offset is not None:
            print(f"  {field} = 0x{offset:X} ({offset})")
    print()

print("=== HAZIR ===")
print("Bu değerleri bana yapıştır, ESP kodunu güncelleyeyim.")
input("Çıkmak için Enter...")