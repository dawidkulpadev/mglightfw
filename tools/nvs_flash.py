# MioGiapicco devices ESP32 based nvs image flashing tool
# Copyright (C) 2026  Dawid Kulpa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Please feel free to contact me at any time by email <dawidkulpadev@gmail.com>


import sys
import os
import argparse
import configparser
import subprocess

def load_config(config_path):
    if not os.path.exists(config_path):
        print(f"[Error] Configuration file missing: {config_path}")
        sys.exit(1)

    config = configparser.ConfigParser()
    try:
        config.read(config_path)
    except Exception as e:
        print(f"[Error] Failed to read configuration file: {e}")
        sys.exit(1)

    if 'flash' not in config:
        print(f"[Error] File {config_path} must contain a [flash] section.")
        sys.exit(1)


    if 'offset' not in config['flash']:
        print("[Error] Section [flash] must contain the 'offset' key.")
        sys.exit(1)

    return config['flash']

def run_esptool(port, baud, offset, bin_path):
    if not os.path.exists(bin_path):
        print(f"[Error] Binary file does not exist: {bin_path}")
        sys.exit(1)

    cmd = [
        "python3",
        "-m", "esptool",
        "--port", port,
        "--baud", baud,
        "write_flash",
        offset,
        bin_path
    ]

    print(f"[*] Starting flash process...")
    print(f"    Port: {port}")
    print(f"    Offset: {offset}")
    print(f"    File: {bin_path}")
    print(f"    Command: {' '.join(cmd)}")
    print("-" * 40)

    try:
        subprocess.check_call(cmd)
        print("-" * 40)
        print(f"[+] Success! File {bin_path} has been flashed.")
    except subprocess.CalledProcessError as e:
        print(f"[Error] Esptool exited with error (code: {e.returncode}).")
        sys.exit(1)
    except FileNotFoundError:
        print(f"[Error] Command 'python3' not found in system.")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Tool for flashing NVS partition (independent of PlatformIO)")

    parser.add_argument("bin_file", help="Path to the .bin file with NVS data")
    parser.add_argument("--port", required=True, help="ESP32 serial port (e.g., /dev/ttyUSB0)")

    parser.add_argument("--config", default="./config.ini", help="Path to configuration file (default: ./config.ini)")

    args = parser.parse_args()


    flash_config = load_config(args.config)

    offset = flash_config['offset']
    baud = flash_config.get('baud', '460800')


    run_esptool(args.port, baud, offset, args.bin_file)

if __name__ == "__main__":
    main()