# Klucze NVM:
## Przestrzeń cert
* pc_sign - Product certificate sign
* manu_pub - Manufacture public key
* dev_pub - devices public key
* dev_priv - devices private key


Product certificate:
- Product generation    (string - liczba)
- Device ID             (devices mac address) (base64)
- Devices public key    (base64)


Wszystko przesyłane kanałem DataRx i DataTx musi być obsługiwane przez "std::string" (ascii) (przed zaszyfrowaniem)
W KeyTx i KeyRx pierwsza przesyłana wartość jest kluczem jako surowe bajty ale potem wszystko musi być w formacie ascii
i obsługiwane przez std::string (przed zaszyfrowaniem). Wszystkie surowe ciągi bajtów enkoduje się base64

Każde urządzenie wymaga wgrania swojego unikalnego klucza prywatnego i publicznego oraz klucza publicznego
certyfikatu producenta.

## Generowanie kluczy:
1. Najpierw trzeba mieć klucze producenta. Do stworzenia trzeba użyć skryptu z katalogu _firmwares_
```
./provision_device.sh init-ca
```


2. Potem trzeba wygenerować klucze dla produktu i podpisać certyfikat urządzenia.
Do tego potrzebny jest adres mac urządzenia. Gdy już mamy to
generujemy plik csv obrazu pamięci nieulotnej który zawiera klucze i podpis certyfikatu.
Skrypt jest w katalogu _tools_ oprogramowania do light.
```
python3 ./generate_nvs_csv.py --ca_key ../ca/ca_key.pem --mac 70:04:1D:26:2C:B0
```


3. Teraz plik csv konwertujemy do pliku bin i wgrywamy do urządzenia. Ten etap dzieje się przy wgrywaniu kodu przez
platformio ale żeby zadziałało to plik csv należy zmienić nazwę na "nvs_data.csv".