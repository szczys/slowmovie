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
