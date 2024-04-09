import WebSerial from '/libs/stm-serial-flasher/api/WebSerial.js';
import settings from '/libs/stm-serial-flasher/api/Settings.js';
import tools from '/libs/stm-serial-flasher/tools.js';

import { osm_flash_api_t } from './flash_apis.js';
import { disable_interaction } from './disable.js';

class flash_controller_base_t {
    constructor(params) {
        this.port = params.port;
        this.api_type = params.api_type;
        this.baudrate = params.baudrate;
        this.flash_firmware = this.flash_firmware.bind(this);
    }

    static write_data(api, records) {
        // eslint-disable-next-line no-async-promise-executor
        return new Promise(async (resolve, reject) => {
            for (const rec of records) {
                await api.write(rec.data, rec.address);
            }
            resolve();
        });
    }

    static flash_start(stm_api) {
        return new Promise((resolve, reject) => {
            let deviceInfo = {
                family: '-',
                bl: '-',
                pid: '-',
                commands: [],
            };

            stm_api.connect({ baudrate: 115200, replyMode: false })
                .then(() => stm_api.cmdGET())
                .then((info) => {
                    deviceInfo = {
                        bl: info.blVersion,
                        commands: info.commands,
                        family: info.getFamily(),
                    };
                })
                .then(() => {
                    let pid;
                    if (deviceInfo.family === 'STM32') {
                        pid = stm_api.cmdGID();
                    } else {
                        pid = '-';
                    }
                    deviceInfo.pid = pid;
                    return pid;
                })
                .then(resolve)
                .catch(reject);
        });
    }

    flash_firmware(fw_bin) {
        const loader = document.getElementById('loader');
        loader.style.display = 'block';
        const disabled = disable_interaction(true);
        if (disabled) {
            let stm_api;
            let serial;
            this.port.close()
                .then(() => {
                    serial = new WebSerial(this.port);
                    serial.onConnect = () => {};
                    serial.onDisconnect = () => {};
                })
                .then(() => { stm_api = new osm_flash_api_t(serial); })
                .then(() => flash_controller_base_t.flash_start(stm_api))
                .then(() => stm_api.eraseAll())
                .then(() => {
                    const records = this.get_records(fw_bin);
                    return flash_controller_base_t.write_data(stm_api, records);
                })
                .then(() => stm_api.disconnect())
                .then(() => {
                    this.port.open({ baudRate: 115200 });
                    loader.style.display = 'none';
                    disable_interaction(false);
                });
        }
    }
}

class flash_controller_t extends flash_controller_base_t {
    constructor(port) {
        super({
            port,
            api_type: osm_flash_api_t,
            baudrate: 115200,
            });
        this.PAGE_SIZE = 0x800;
        this.SIZE_BOOTLOADER = 2 * this.PAGE_SIZE;
        this.SIZE_CONFIG = 2 * this.PAGE_SIZE;
        this.ADDRESS_BOOTLOADER = parseInt(settings.startAddress, 16);
        this.ADDRESS_CONFIG = this.ADDRESS_BOOTLOADER + this.SIZE_BOOTLOADER;
        this.ADDRESS_FIRMWARE = this.ADDRESS_CONFIG + this.SIZE_CONFIG;
    }

    get_records(data) {
        const records = [{
                data: new Uint8Array(data.slice(0, this.SIZE_BOOTLOADER)),
                address: this.ADDRESS_BOOTLOADER,
            },
            {
                data: new Uint8Array(
                    data.slice(this.ADDRESS_FIRMWARE - this.ADDRESS_BOOTLOADER, -1),
                ),
                address: this.ADDRESS_FIRMWARE,
            }];
        return records;
    }
}

export class firmware_t {
    constructor(dev) {
        this.dev = dev;
        this.create_firmware_table = this.create_firmware_table.bind(this);
        this.flash_latest = this.flash_latest.bind(this);
    }

    async create_firmware_table(fw_info) {
        await disable_interaction(true);
        const json_fw = fw_info;
        const tablediv = document.getElementById('home-firmware-table');

        const tbl = tablediv.appendChild(document.createElement('table'));
        const body = tbl.createTBody();
        tbl.createTHead();

        const title = tbl.tHead.insertRow();
        const title_cell = title.insertCell();
        title_cell.textContent = 'Latest Firmware Available';

        for (const [key, value] of Object.entries(json_fw)) {
            if (key !== 'path') {
                const fw_row = body.insertRow();
                const keyh = fw_row.insertCell();
                let key_f;
                if (key === 'sha') {
                    key_f = key.toUpperCase();
                } else {
                    key_f = key.charAt(0).toUpperCase() + key.slice(1);
                }
                keyh.textContent = `${key_f}: ${value}`;
            }
        }

        const flash_btn = document.getElementById('fw-btn');
        flash_btn.style.display = 'block';
        flash_btn.addEventListener('click', () => { this.flash_latest(fw_info); });

        await disable_interaction(false);
    }

    get_latest_firmware_info(model) {
        fetch('../../fw_releases/latest_fw_info.json')
            .then((resp) => resp.json())
            .then((json) => {
                const fw_entry = json.find(element => {
                    return element.path === `${model}_release.bin`;
                })
                if (!fw_entry)
                    return;
                this.create_firmware_table(fw_entry);
            })
            .catch((err) => {
                console.log(err);
            });
    }

    flash_latest(fw_info) {
        const { port } = this.dev;
        if (window.confirm('Are you sure you want to update the firmware?')) {
            const fw_path = fw_info.path;
            fetch(`../../${fw_path}`)
                .then((r) => r.blob())
                .then((resp) => {
                    const reader = new FileReader();
                    reader.onload = (e) => {
                        const fw_bin = Uint8Array.from(e.target.result, (c) => c.charCodeAt(0));
                        const controller = new flash_controller_t(port);
                        controller.flash_firmware(fw_bin);
                    };
                    reader.onerror = (e) => {
                        console.log(e);
                    };
                    reader.readAsBinaryString(resp);
                });
            }
        }
    }
