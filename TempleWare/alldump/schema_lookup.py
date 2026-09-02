import json
import os

def resolve_field(class_name, field_name, classes):
    """Field'ı sınıfın kendisinde veya parent zincirinde ara."""
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

# client_dll.json dosyasını aç
file_path = "client_dll.json"
if not os.path.exists(file_path):
    print(f"{file_path} bulunamadı! Bu klasörde olduğundan emin ol.")
    exit(1)

with open(file_path, "r", encoding="utf-8") as f:
    data = json.load(f)

classes = data.get("classes", {})
if not classes:
    print("client_dll.json içinde 'classes' anahtarı bulunamadı.")
    exit(1)

# Aranacak sınıflar
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

# Aranacak alanlar
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

print("=== CLIENT.DLL SİSTEMİ ===")
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

# Ayrıca CEntityInstance boyutunu bul (eğer varsa)
for potential in ["CEntityInstance", "CBaseEntity", "C_BaseEntity"]:
    if potential in classes:
        # Boyut bazen "size" alanında olabilir, ama şemada olmayabilir.
        # Stride olarak 0x78 kullanıyoruz ama bilgi varsa göster
        print(f"Not: {potential} sınıfı bulundu. Boyut bilgisi JSON'da genelde yok; stride 0x78 varsayıldı.")
        break

print("=== HAZIR ===")
print("Yukarıdaki değerleri bana yapıştır, kodu güncelleyeyim.")
input("Çıkmak için Enter'a bas...")