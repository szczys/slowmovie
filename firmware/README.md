# Slowmovie Firmware

The Slowmovie firmware is built using ESP-IDF for an ESP32s3 connected
to an 800x480, 3-color ePaper display.

The display was available for a brief time on AliExpress for $33.25,
labelled "HINK-E075A22-A0 7.5inch" but is no longer available. The
firmware uses the GxEPD2 driver which includes support for a wide number
of displays so you can easily update to support different displays.

## Provisioning

The device must be provisioned with:
- WiFi SSID and PSK (password)
- x509 certificates for connecting to Golioth

Credentials may be stored on the device using any of two approaches:
- Use the Console to store credentials
- Generate an NVS binary with credentials and flash to the device

### Set Credentials via Console

The serial console includes commands to store all required credentials.
Use the "help" command for example usage.

1. Store WiFi Credentials

    WiFi credentials are stored as simple strings:

    ```
    ssid <your_ssid>
    psk <your_psk>
    ```

2. Store Device Credentials

    Device credentials are submitted as base64-encoding for the DER
    files. Command line tools may be used for the encoding:

    ```
    base64 --wrap=0 device.crt.der
    base64 --wrap=0 device.key.der
    ```

    Paste the resulting string as part of the console commands

    ```
    crt <paste_your_base64_encoded_device_crt_der>
    key <paste_your_base64_encoded_device_key_der>
    ```

### Generate and Flash NVS Storage Partition

Credentials are written to the device separately from flashing the
firmware. This is accomplished by generating a settings partition and
writing it to the ESP32s3 using esptool.py.

1. Install Python Tools

  If you have ESP-IDF installed, the necessary Python tools are already
  installed. Otherwise, they may be installed using `pip`.

  ```
  pip install esp-idf-nvs-partition-gen esptool
  ```

2. Position Credentials as CSV

  Generate an x509 certificate and upload the public CA cert to Golioth.

  - [Golioth PKI
    documentation](https://docs.golioth.io/connectivity/credentials/pki/offline-pki)

  Create a file called `nvs.csv` that includes WiFi credentials and
  location of your DER-formatted device certificate and device private
  key.

  ```csv
  key,type,encoding,value
  slowmovie_creds,namespace,,
  wifi_ssid,data,string,"your_wifi_ssid"
  wifi_psk,data,string,"your_wifi_password"
  crt_der,file,binary,/path/to/device.crt.der
  key_der,file,binary,/path/to/device.key.der
  ```

  Change only the final portion of each of the last four lines to match
  your credentials.

3. Generate the NVS Binary

  ```
  python -m esp_idf_nvs_partition_gen generate nvs.csv nvs.bin 24576
  ```

4. Write the Binary to Device

  ```
  esptool.py write_flash 0x9000 nvs.bin
  ```
