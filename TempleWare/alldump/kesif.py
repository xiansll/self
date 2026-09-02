import json
import os

file_path = "client_dll.json"
if not os.path.exists(file_path):
    print("client_dll.json bulunamadı!")
    input()
    exit()

with open(file_path, "r", encoding="utf-8") as f:
    data = json.load(f)

print("Üst düzey tip:", type(data).__name__)
print()

if isinstance(data, dict):
    keys = list(data.keys())
    print(f"Toplam üst anahtar sayısı: {len(keys)}")
    print("İlk 30 anahtar:")
    for k in keys[:30]:
        print("  -", k)
    
    print()
    # Her üst anahtarın altındaki yapıyı göster
    for k in keys[:5]:
        print(f"Üst anahtar: {k}")
        val = data[k]
        print(f"  Değer tipi: {type(val).__name__}")
        if isinstance(val, dict):
            sub_keys = list(val.keys())
            print(f"  Alt anahtarlar ({len(sub_keys)} adet, ilk 10):")
            for sk in sub_keys[:10]:
                print(f"    * {sk}")
        elif isinstance(val, list):
            print(f"  Liste uzunluğu: {len(val)}")
            if val and isinstance(val[0], dict):
                print(f"  İlk elemanın anahtarları: {list(val[0].keys())[:10]}")
        print()
elif isinstance(data, list):
    print(f"Liste uzunluğu: {len(data)}")
    if data and isinstance(data[0], dict):
        print("İlk elemanın anahtarları:", list(data[0].keys()))
        print("İlk elemanın tam içeriği (ilk 500 karakter):")
        print(json.dumps(data[0], indent=2)[:500])

print("=== KEŞİF TAMAMLANDI ===")
print("Bu çıktıyı bana yapıştır, doğru yolu bulup asıl offsetleri çıkaralım.")
input("Çıkmak için Enter...")