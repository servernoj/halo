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
PS: replace `<username>` with your actual username on RasPi

Initialize the unit
```sh
sudo systemctl daemon-reload
sudo systemctl start halo.service
```

