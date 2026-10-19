# ESP32 eduroam lab

Prepared for this CachyOS laptop and the existing ESP32 DevKit project in Coder:
`/home/coder/projects/RPI I/Practica 2/wifi_enterprise`.
The same laptop lab files are mounted in Coder at `/home/coder/git/eduroam-lab`.

The laptop runs hostapd, FreeRADIUS 3.x, and dnsmasq. Run the AP scripts in a **laptop terminal, outside Docker/Coder**. The container does not have the host's Wi-Fi device.

## Start and restore

```bash
cd /home/fixt/git/eduroam-lab
./install-dependencies.sh
sudo ./turn-on.sh
# When finished:
sudo ./go-back-to-normal.sh
```

The installer needs your sudo password. The start script uses `wlan0` and switches it from your normal Wi-Fi connection into AP mode; your existing Wi-Fi connection is disconnected while the lab is on. Open the laptop terminal and prepare/flash the board before switching. A Coder page addressed using the laptop's old Wi-Fi IP may disconnect during the switch. The restore script stops only the lab's three transient systemd services, returns the adapter to NetworkManager, and reconnects the saved Wi-Fi profile. Failed startup automatically attempts the same restoration. If reconnection fails, state is retained so you can run restore again.

| Setting | Value |
| --- | --- |
| SSID | `eduroam` |
| Security | WPA2-Enterprise / AES-CCMP |
| EAP / phase 2 | TTLS / PAP |
| Outer identity | `anonymous@ucm.es` |
| Username | `serrom04@ucm.es` |
| Password | Supplied lab password, already set in RADIUS and ESP-IDF |
| AP / gateway | `172.16.81.1` |
| Netmask | `255.255.255.0` |
| DHCP range | `172.16.81.100`–`172.16.81.150` |
| Band / channel | 2.4 GHz / 6 |
| RADIUS | Loopback only, `127.0.0.1:1812` |
| Server name | `radius.eduroam.lab` |

This recreates eduroam's requested authentication method locally. It is a standalone lab; it does not contact UCM or the eduroam federation. There is no internet forwarding or DNS service. No firewall rules or global service configs are changed. Keep this test AP in your controlled lab area, since its SSID matches campus networks.

## Certificates

`certs/ca.pem` is the **PEM CA certificate** used by the ESP32; it was copied into the Coder project's `main/ca.pem`. `server.pem` and `server.key` are the FreeRADIUS server identity. `ca.key` stays on the laptop. TTLS/PAP does not require a client certificate or client key; the build embeds only the CA.

CA validation and server domain validation are enabled. The firmware explicitly skips certificate date checks because this isolated lab has no clock source before Wi-Fi authentication; it still checks the CA signature and server name. For a deployment with a valid RTC, enable date checks after initializing the clock. These lab certificates are not campus certificates.

## Build and flash from Coder

Open the existing `wifi_enterprise` folder in code-server. Use **Terminal → Run Build Task → ESP-IDF: Build lab** (or Ctrl+Shift+B). The project is configured for `esp32`, WPA2, TTLS/PAP, your supplied identities/credentials, and the lab CA. Its previous settings and source are saved under `lab-backups/20261012-100832/`.

The ESP-IDF Web extension is installed: command palette **ESP-IDF-Web Flash and Monitor** can use your laptop's USB device from the browser, when code-server is served from a browser secure context such as HTTPS or localhost and the browser supports Web Serial.

For a laptop USB fallback, a merged firmware image and local esptool environment are already prepared:

```bash
cd /home/fixt/git/eduroam-lab
./flash-esp32.sh /dev/ttyUSB0
./monitor-esp32.sh /dev/ttyUSB0
```

Replace the port with your board's port; `/dev/serial/by-id/` gives stable names. If your user lacks serial-device permission, run those commands with `sudo`. Hold BOOT while flashing if the DevKit does not enter download mode automatically. The Coder container currently has no USB serial passthrough, so a plain `idf.py flash` inside the container cannot access a laptop USB board.

The merged image is a snapshot of the build completed during setup. After later project changes, rebuild and refresh it in the Coder terminal:

```bash
cd '/home/coder/projects/RPI I/Practica 2/wifi_enterprise/build'
/opt/esp/python_env/idf6.1_py3.12_env/bin/python3 -m esptool --chip esp32 merge-bin \
  -o /home/coder/git/eduroam-lab/firmware/esp32-eduroam.bin @flash_args
```

## Confirm connection

After starting the AP, reset the flashed board. Its serial monitor should report an IP in `172.16.81.100–150`, mask `255.255.255.0`, gateway `172.16.81.1`.

```bash
sudo journalctl -f -u eduroam-lab-radius -u eduroam-lab-hostapd -u eduroam-lab-dhcp
sudo cat /run/eduroam-lab/dnsmasq.leases
sudo hostapd_cli -p /run/eduroam-lab/hostapd -i wlan0 all_sta
```

If a host firewall is enabled later, allow inbound DHCP UDP 67 on wlan0. RADIUS is only loopback; do not expose it to the wireless network.

## Validation performed

- ESP-IDF 6.1 compiled the configured ESP32 firmware successfully.
- OpenSSL verified the generated server certificate against the lab CA for server authentication.
- FreeRADIUS 3.2.10 accepted the config. An actual EAP-TTLS/PAP exchange using `eapol_test` succeeded with the supplied credentials. Wrong-password and wrong-server-domain exchanges were rejected.
- hostapd parsed the AP config; dnsmasq accepted the DHCP config; Bash syntax checks passed.
- Mocked script integration passed startup, restore/reconnection, repeated restoration, and failed-start rollback without operating the host radio.
- Radio/AP startup and on-board association are still to be tested: root access requires your sudo password, and no USB serial board was attached during setup.

Reference: [Espressif Wi-Fi Enterprise example](https://github.com/espressif/esp-idf/tree/master/examples/wifi/wifi_enterprise).
