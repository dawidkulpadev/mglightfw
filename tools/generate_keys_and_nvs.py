import sys
import os
import argparse
import base64
import csv
import binascii
import re
import configparser
import subprocess
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils


def load_config(config_path):
    if not os.path.exists(config_path):
        print(f"[Error] Configuration file missing at path: {config_path}")
        print(f"Ensure the file exists or provide the correct path using the --config argument.")
        sys.exit(1)

    config = configparser.ConfigParser()
    try:
        config.read(config_path)
    except Exception as e:
        print(f"[Error] Failed to read configuration file: {e}")
        sys.exit(1)

    if 'nvs' not in config or 'tool_path' not in config['nvs']:
        print(f"[Error] File {config_path} must contain section [nvs] and key 'tool_path'.")
        sys.exit(1)

    return config['nvs']

def int_to_bytes(val, length=32):
    return val.to_bytes(length, byteorder='big')

def to_hex(b):
    return binascii.hexlify(b).decode('utf-8')

def load_pem_private_key(filename):
    try:
        with open(filename, 'rb') as f:
            pem_data = f.read()
            private_key = serialization.load_pem_private_key(pem_data, password=None)
            if not isinstance(private_key.curve, ec.SECP256R1):
                raise ValueError("Key must be on the SECP256R1 (P-256) curve")
            return private_key
    except Exception as e:
        print(f"[Error] Loading PEM key {filename}: {e}")
        sys.exit(1)

def run_nvs_generator(tool_path, csv_path, bin_path, size):
    if not os.path.exists(tool_path):
        print(f"[Error] NVS tool not found at path: {tool_path}")
        print("Check the 'tool_path' setting in the configuration file.")
        sys.exit(1)

    cmd = [
        sys.executable,
        tool_path,
        "generate",
        csv_path,
        bin_path,
        size
    ]

    print(f"[*] Generating NVS binary...")
    print(f"    Command: {' '.join(cmd)}")

    try:
        subprocess.check_call(cmd)
        print(f"[+] Success! Generated binary file: {bin_path}")
    except subprocess.CalledProcessError as e:
        print(f"[Error] NVS generator returned error: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Key and NVS image generator for ESP32")

    parser.add_argument("--ca_key", required=True, help="Path to ca_key.pem file")
    parser.add_argument("--mac", required=True, help="MAC address (e.g., AA:BB:CC:DD:EE:FF)")

    parser.add_argument("--out", required=False, help="Optional output filename (without extension)")
    parser.add_argument("--config", required=False, default="./config.ini",
                        help="Path to configuration file (default: ./config.ini)")

    args = parser.parse_args()


    print(f"[*] Loading configuration from: {args.config}")
    nvs_config = load_config(args.config)

    nvs_tool = nvs_config['tool_path']
    nvs_size = nvs_config.get('partition_size', '0x5000')


    clean_mac_hex = re.sub(r'[^a-fA-F0-9]', '', args.mac).lower()

    if args.out:
        base_name = os.path.splitext(args.out)[0]
    else:
        base_name = clean_mac_hex

    csv_filename = f"{base_name}.csv"
    bin_filename = f"{base_name}.bin"

    print(f"--- Generating data for MAC: {args.mac} ---")


    manu_priv_key = load_pem_private_key(args.ca_key)
    manu_pub_key = manu_priv_key.public_key()
    manu_pub_nums = manu_pub_key.public_numbers()
    manu_pub_bytes = int_to_bytes(manu_pub_nums.x) + int_to_bytes(manu_pub_nums.y)


    dev_priv_key = ec.generate_private_key(ec.SECP256R1())
    dev_pub_key = dev_priv_key.public_key()

    dev_priv_val = dev_priv_key.private_numbers().private_value
    dev_priv_bytes = int_to_bytes(dev_priv_val)

    dev_pub_nums = dev_pub_key.public_numbers()
    dev_pub_bytes = int_to_bytes(dev_pub_nums.x) + int_to_bytes(dev_pub_nums.y)


    mac_b64 = base64.b64encode(args.mac.encode('utf-8')).decode('utf-8')
    dev_pub_b64 = base64.b64encode(dev_pub_bytes).decode('utf-8')

    payload_str = f"2;{mac_b64};{dev_pub_b64}"
    payload_bytes = payload_str.encode('utf-8')

    print(f"[*] Data to sign: {payload_str}")

    signature_der = manu_priv_key.sign(payload_bytes, ec.ECDSA(hashes.SHA256()))
    r, s = utils.decode_dss_signature(signature_der)
    signature_raw = int_to_bytes(r) + int_to_bytes(s)


    csv_rows = [
        ["key", "type", "encoding", "value"],
        ["cert", "namespace", "", ""],
        ["pc_sign", "data", "hex2bin", to_hex(signature_raw)],
        ["manu_pub", "data", "hex2bin", to_hex(manu_pub_bytes)],
        ["dev_priv", "data", "hex2bin", to_hex(dev_priv_bytes)],
        ["dev_pub", "data", "hex2bin", to_hex(dev_pub_bytes)]
    ]

    try:
        with open(csv_filename, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile, quoting=csv.QUOTE_MINIMAL)
            writer.writerows(csv_rows)
        print(f"[+] Created CSV: {csv_filename}")
    except Exception as e:
        print(f"[Error] Failed to write CSV: {e}")
        sys.exit(1)


    run_nvs_generator(nvs_tool, csv_filename, bin_filename, nvs_size)

if __name__ == "__main__":
    main()