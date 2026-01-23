Import("env")
import os

proj = env["PROJECT_DIR"]
bin_path = os.path.join(proj, "tools", "mfg_nvs.bin")

# OFFSET MUSI PASOWAĆ DO partitions.csv
MFG_NVS_OFFSET = "0x9000"

def flash_mfg_nvs(target, source, env):
    if not os.path.exists(bin_path):
        print("WARNING: brak tools/mfg_nvs.bin – pomijam flash mfg_nvs")
        return

    port = env.subst("$UPLOAD_PORT")
    speed = env.subst("$UPLOAD_SPEED") or "460800"

    cmd = (
        f'./tools/venv/bin/python3 -m esptool '
        f'--port {port} '
        f'--baud {speed} '
        f'write-flash {MFG_NVS_OFFSET} "{bin_path}"'
    )

    print("Flashing mfg_nvs:")
    print(cmd)

    env.Execute(cmd)

# ⬇⬇⬇ TO JEST WAŻNE ⬇⬇⬇
env.AddPostAction("upload", flash_mfg_nvs)
