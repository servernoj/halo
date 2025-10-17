# Install the service

## Install NVM + Node + PNPM

Follow steps from https://github.com/nvm-sh/nvm?tab=readme-ov-file#installing-and-updating to install NVM

Then install `Node 22` and `pnpm`
```sh
nvm install 22
nvm alias default 22
npm i -g npm
```

## Install GitHub actions runner

Follow steps in `https://github.com/<username>/<repo>/settings/actions/runners/new`

## Install systemd service

Create file `/etc/systemd/system/halo.service` with the following content

```` ini
[Unit]
Description=Halo API Service
After=network.target

[Service]
Type=simple
User=<username>
WorkingDirectory=/home/<username>/halo
ExecStart=/bin/bash -lc 'source $HOME/.nvm/nvm.sh && pnpm api start'
Restart=on-failure
RestartSec=5
Environment=NODE_ENV=production

[Install]
WantedBy=multi-user.target
````
**Note**: replace `<username>` with your actual username on RasPi

Initialize the unit
```sh
sudo systemctl daemon-reload
sudo systemctl start halo.service
```
## Logging ESP32 console

Connect `esp32c6` with `RasPi` via USB that doesn't pass VCC signal.

Create `systemd` config `/etc/systemd/system/esp32-logger.service`:
```` ini
[Unit]
Description=ESP32 Serial Logger
After=multi-user.target
[Service]
Type=simple
ExecStart=/bin/bash -c 'cat <USB-Serial Device path> | systemd-cat -t esp32 -p info'
Restart=always
RestartSec=5
[Install]
WantedBy=multi-user.target
````
**Note**: Replace `<USB-Serial Device path>` with the actual device path.

Load and start the new service:
```` sh
sudo systemctl daemon-reload
sudo systemctl enable esp32-logger
sudo systemctl start esp32-logger
````

Use either of the commands to work with the log:
```` sh
# View recent logs
sudo journalctl -u esp32-logger -f
# View logs from specific date
sudo journalctl -u esp32-logger --since "2025-10-01" --until "2025-10-02"
# Force rotation
sudo journalctl --rotate
# Vacuum old logs
sudo journalctl --vacuum-time=30d
sudo journalctl --vacuum-size=1G
````