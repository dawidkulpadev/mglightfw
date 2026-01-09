Import("env")
import os
import sys
import glob
import subprocess

proj = env["PROJECT_DIR"]
tools_dir = os.path.join(proj, "tools")
csv_path = os.path.join(tools_dir, "nvs_data.csv")
out_bin = os.path.join(tools_dir, "mfg_nvs.bin")

# Rozmiar binarki = rozmiar partycji mfg_nvs z partitions_custom.csv
NVS_SIZE = "0x6000"

def find_nvs_partition_gen():
    # Najczęściej w paczce framework-espidf w: components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py
    pio_home = os.path.expanduser("~/.platformio")
    candidates = glob.glob(os.path.join(pio_home, "packages", "framework-espidf", "components",
                                        "nvs_flash", "nvs_partition_generator", "nvs_partition_gen.py"))
    if candidates:
        return candidates[0]
    # Czasem może być w toolchainach/paczkach – zostawiamy prosty fallback:
    candidates = glob.glob(os.path.join(pio_home, "packages", "**", "nvs_partition_gen.py"), recursive=True)
    return candidates[0] if candidates else None

def gen_nvs(source, target, env_):
    gen_py = find_nvs_partition_gen()
    if not gen_py:
        print("ERROR: Nie znalazłem nvs_partition_gen.py w paczkach PlatformIO.")
        print("Szukaj w ~/.platformio/packages/framework-espidf/components/nvs_flash/...")
        env.Exit(1)

    if not os.path.exists(csv_path):
        print(f"ERROR: Brak pliku CSV: {csv_path}")
        env.Exit(1)

    cmd = [
        sys.executable, gen_py,
        "generate",
        csv_path,
        out_bin,
        NVS_SIZE
    ]
    print("Generating NVS bin:", " ".join(cmd))
    subprocess.check_call(cmd)

gen_nvs(None, None, env)