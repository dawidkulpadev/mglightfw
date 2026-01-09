Import("env")
import os

proj = env["PROJECT_DIR"]
bin_path = os.path.join(proj, "tools", "mfg_nvs.bin")

# Offset partycji mfg_nvs z partitions_custom.csv:
MFG_NVS_OFFSET = "0x150000"

def flash_mfg_nvs(source, target, env_):
    if not os.path.exists(bin_path):
        print("WARNING: brak tools/mfg_nvs.bin – pomijam flash mfg_nvs")
        return

    port = env.subst("$UPLOAD_PORT")
    speed = env.subst("$UPLOAD_SPEED") or "460800"

    # esptool z PlatformIO:
    # $PYTHONEXE -m esptool ... działa, bo PlatformIO ma esptool w środowisku uploadu
    cmd = f'$PYTHONEXE -m esptool --port {port} --baud {speed} write_flash {MFG_NVS_OFFSET} "{bin_path}"'
    print("Flashing mfg_nvs:", cmd)
    env.Execute(cmd)

# Uruchom po "upload"
env.AddPostAction("upload", flash_mfg_nvs)