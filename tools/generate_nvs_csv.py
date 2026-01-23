#!/usr/bin/env python3
import sys
import argparse
import base64
import csv
import binascii
import re
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

# Funkcja pomocnicza do konwersji int do bytes (big endian, 32 bajty)
def int_to_bytes(val, length=32):
    return val.to_bytes(length, byteorder='big')

def load_pem_private_key(filename):
    """Wczytuje klucz prywatny z formatu PEM."""
    try:
        with open(filename, 'rb') as f:
            pem_data = f.read()
            private_key = serialization.load_pem_private_key(
                pem_data,
                password=None
            )
            if not isinstance(private_key.curve, ec.SECP256R1):
                raise ValueError("Klucz musi być na krzywej SECP256R1 (P-256)")
            return private_key
    except Exception as e:
        print(f"Błąd wczytywania klucza PEM {filename}: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Generator kluczy ESP32 z automatycznym nazewnictwem")
    parser.add_argument("--ca_key", required=True, help="Ścieżka do pliku ca_key.pem")
    parser.add_argument("--mac", required=True, help="Adres MAC (np. AA:BB:CC:DD:EE:FF)")
    # Argument --out jest teraz opcjonalny (nargs='?' oznacza 0 lub 1 argument)
    parser.add_argument("--out", required=False, help="Nazwa pliku wyjściowego (opcjonalna, domyślnie: mac_hex.csv)")

    args = parser.parse_args()

    # 1. Przetwarzanie nazwy pliku wyjściowego
    # Usuwamy dwukropki, myślniki i spacje, zamieniamy na małe litery
    clean_mac_hex = re.sub(r'[^a-fA-F0-9]', '', args.mac).lower()

    if len(clean_mac_hex) != 12:
        print(f"[Ostrzeżenie] Adres MAC po oczyszczeniu ma {len(clean_mac_hex)} znaków (oczekiwano 12 dla standardowego MAC).")

    if args.out:
        output_filename = args.out
    else:
        # Generowanie nazwy na podstawie MAC
        output_filename = f"{clean_mac_hex}.csv"

    # 2. Wczytanie klucza prywatnego CA
    print(f"[*] Wczytuję klucz CA z: {args.ca_key}")
    manu_priv_key = load_pem_private_key(args.ca_key)

    # Wydobycie klucza publicznego CA
    manu_pub_key = manu_priv_key.public_key()
    manu_pub_nums = manu_pub_key.public_numbers()
    manu_pub_bytes = int_to_bytes(manu_pub_nums.x) + int_to_bytes(manu_pub_nums.y)

    # 3. Generowanie nowej pary kluczy dla urządzenia
    print("[*] Generuję nową parę kluczy P-256 dla urządzenia...")
    dev_priv_key = ec.generate_private_key(ec.SECP256R1())
    dev_pub_key = dev_priv_key.public_key()

    dev_priv_val = dev_priv_key.private_numbers().private_value
    dev_priv_bytes = int_to_bytes(dev_priv_val)

    dev_pub_nums = dev_pub_key.public_numbers()
    dev_pub_bytes = int_to_bytes(dev_pub_nums.x) + int_to_bytes(dev_pub_nums.y)

    # 4. Kodowanie Base64
    mac_b64 = base64.b64encode(args.mac.encode('utf-8')).decode('utf-8')
    dev_pub_b64 = base64.b64encode(dev_pub_bytes).decode('utf-8')

    # Format ciągu do podpisania: 2;MAC_BASE64;DEV_PUB_BASE64
    payload_str = f"2;{mac_b64};{dev_pub_b64}"
    payload_bytes = payload_str.encode('utf-8')

    print(f"[*] Dane do podpisania: {payload_str}")

    # 5. Podpisywanie
    signature_der = manu_priv_key.sign(
        payload_bytes,
        ec.ECDSA(hashes.SHA256())
    )

    r, s = utils.decode_dss_signature(signature_der)
    signature_raw = int_to_bytes(r) + int_to_bytes(s)

    # 6. Zapis do CSV
    def to_hex(b):
        return binascii.hexlify(b).decode('utf-8')

    csv_rows = [
        ["key", "type", "encoding", "value"],
        ["cert", "namespace", "", ""],
        ["pc_sign", "data", "hex2bin", to_hex(signature_raw)],
        ["manu_pub", "data", "hex2bin", to_hex(manu_pub_bytes)],
        ["dev_priv", "data", "hex2bin", to_hex(dev_priv_bytes)],
        ["dev_pub", "data", "hex2bin", to_hex(dev_pub_bytes)]
    ]

    try:
        with open(output_filename, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile, quoting=csv.QUOTE_MINIMAL)
            writer.writerows(csv_rows)
        print(f"[+] Sukces! Plik utworzony: {output_filename}")
    except Exception as e:
        print(f"Błąd zapisu pliku CSV: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()