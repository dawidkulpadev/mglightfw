# MioGiapicco Light Firmware Gen 2
Firmware for the ESP32-C3 chip of the MioGiapicco Light device, version 4 or higher.



Wszystko przesyłane kanałem DataRx i DataTx musi być obsługiwane przez "std::string" (ascii) (przed zaszyfrowaniem)
W KeyTx i KeyRx pierwsza przesyłana wartość jest kluczem jako surowe bajty ale potem wszystko musi być w formacie ascii
i obsługiwane przez std::string (przed zaszyfrowaniem). Wszystkie surowe ciągi bajtów enkoduje się base64

Każde urządzenie wymaga wgrania swojego unikalnego klucza prywatnego i publicznego oraz klucza publicznego
certyfikatu producenta.

### Generowanie kluczy:
1. Create _config.ini_ file with following fields / keys
```ini
[nvs]
tool_path = ~/.platformio/packages/framework-espidf/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py
partition_size = 0x5000

[flash]
offset = 0x9000
baud = 460800
```


2. Generate P-256 keys and binary nvs partition image with generate_keys_and_nvs.py
```text
usage: generate_keys_and_nvs.py [-h] --ca_key CA_KEY --mac MAC [--out OUT] [--config CONFIG]

options:
  -h, --help       show this help message and exit
  --ca_key CA_KEY  Path to ca_key.pem file
  --mac MAC        MAC address (e.g., AA:BB:CC:DD:EE:FF)
  --out OUT        Optional output filename (without extension)
  --config CONFIG  Path to configuration file (default: ./config.ini)

example:
  python3 ./generate_keys_and_nvs.py --ca_key ../../ca/ca_key.pem --mac 70:04:1D:26:2C:B0
```


3. Flash nvs partition image with nvs_flash.py
```text
usage: nvs_flash.py [-h] --port PORT [--config CONFIG] bin_file

positional arguments:
  bin_file         Path to the .bin file with NVS data/image

options:
  -h, --help       show this help message and exit
  --port PORT      ESP32 serial port (e.g., /dev/ttyUSB0)
  --config CONFIG  Path to configuration file (default: ./config.ini)

example:
  python3 ./nvs_flash.py --port /dev/ttyUSB0 ./70041d262cb0.bin
```

# Architecture

## Device certificate
An ASCII string consisting of the following values separated by semicolons:
- Product generation - number
- Device ID - devices mac address - hex representation of bytes - base64 encoded
- Devices public key - hex representation of bytes - base64 encoded

## Settings - NVM
### Authorization settings
Namespace: _cert_  
Keys:
- pc_sign - Product certificate sign _[bytes]_
- manu_pub - Manufacture public key _[bytes]_
- dev_pub - devices public key _[bytes]_
- dev_priv - devices private key _[bytes]_

### Other settings
Namespace: _mgld_  
Keys:
- psk - WiFi password _[string]_
- ssid - WiFi SSID _[string]_
- picklock - devices password for API login _[string]_
- uid - user ID _[number as string]_
- tz - timezone from tz database _[string]_
- role - Settings value to determine if device should act as _server_, _client_ or should 
  become server when no client in sight (auto) _[one digit number]_
    - 0 - Auto
    - 1 - Server
    - 2 - Client
- dli - Maximum daylight Intensity (percent)
- ds - Day start time in minutes since 00:00
- de - Day end time in minutes since 00:00
- ssd - sunset duration in minutes since 00:00
- srd - sunrise duration in minutes since 00:00
