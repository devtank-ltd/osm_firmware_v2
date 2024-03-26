import WebSerial from '../stm-serial-flasher/src/api/WebSerial.js';
import settings from '../stm-serial-flasher/src/api/Settings.js';

import { osm_flash_api_t } from './flash_apis.js';
import { disable_interaction } from './disable.js';


class flash_controller_t {
    constructor(dev) {
        this.dev = dev;
        this.PAGE_SIZE          = 0x800;
        this.SIZE_BOOTLOADER    = 2 * this.PAGE_SIZE;
        this.SIZE_CONFIG        = 2 * this.PAGE_SIZE;
        this.ADDRESS_BOOTLOADER = parseInt(settings.startAddress);
        this.ADDRESS_CONFIG     = this.ADDRESS_BOOTLOADER + this.SIZE_BOOTLOADER
        this.ADDRESS_FIRMWARE   = this.ADDRESS_CONFIG + this.SIZE_CONFIG;
    }

    flash_start (osmAPI) {
        return new Promise( function (resolve, reject) {
            let deviceInfo = {
                family: '-',
                bl: '-',
                pid: '-',
                commands: [],
            };

            let error = null;
            osmAPI.connect({baudrate: 115200, replyMode: false})
                .then(() => {return osmAPI.cmdGET()})
                .then((info) => {
                    deviceInfo.bl = info.blVersion;
                    deviceInfo.commands = info.commands;
                    deviceInfo.family = info.getFamily();
                })
                .then(() => {
                    let pid;
                    if (deviceInfo.family === 'STM32') {
                        pid = osmAPI.cmdGID();
                    } else {
                        pid = '-';
                    }
                    deviceInfo.pid = pid;
                    return pid;
                })
                .then(resolve)
                .catch(reject);
        })
    }

    flash_firmware(fw_bin) {
        const loader = document.getElementById('loader');
        loader.style.display = 'block';
        let disabled = disable_interaction(true);
        if (disabled) {
            let osmAPI;
            let serial;
            let start_address = parseInt(settings.startAddress);
            this.dev.port.close()
                .then(() => {
                    serial = new WebSerial(this.dev.port)
                    serial.onConnect = () => {};
                    serial.onDisconnect = () => {};
                })
                .then(() => osmAPI = new osm_flash_api_t(serial))
                .then(() => this.flash_start(osmAPI))
                .then(() => osmAPI.eraseAll() )
                .then(async () => {
                    let data_bootloader = new Uint8Array(fw_bin.slice(0, this.SIZE_BOOTLOADER));
                    await osmAPI.write(data_bootloader, this.ADDRESS_BOOTLOADER);

                    let data_firmware = new Uint8Array(fw_bin.slice(this.ADDRESS_FIRMWARE - this.ADDRESS_BOOTLOADER, -1));
                    await osmAPI.write(data_firmware, this.ADDRESS_FIRMWARE);
                })
                .then(() => {
                    return osmAPI.disconnect();
                })
                .then(() => {
                    this.dev.port.open({ "baudRate": 115200 });
                    loader.style.display = 'none';
                    disable_interaction(false);
                });
        }
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
                let fw_row = body.insertRow();
                let keyh = fw_row.insertCell();
                let key_f;
                if (key === 'sha') {
                    key_f = key.toUpperCase();
                }
                else {
                    key_f = key.charAt(0).toUpperCase() + key.slice(1);
                }
                keyh.textContent = `${key_f}: ${value}`;
            }
        }

        const flash_btn = document.getElementById('fw-btn');
        flash_btn.style.display = 'block';
        flash_btn.addEventListener('click', () => { this.flash_latest(fw_info) });

        await disable_interaction(false);
    }

    get_latest_firmware_info() {
        fetch('../../fw_releases/latest_fw_info.json')
            .then((resp) => resp.json())
            .then((json) => {
                this.create_firmware_table(json);
            })
            .catch((err) => {
                ;
            })
    }

    flash_latest(fw_info) {
        const dev = this.dev;
        if (confirm("Are you sure you want to update the firmware?")) {
            const fw_path = fw_info.path;
            fetch(`../../${fw_path}`)
                .then((r) => r.blob())
                .then((resp) => {
                    var reader = new FileReader();
                    reader.onload = function (e) {
                        const fw_bin = Uint8Array.from(e.target.result, c => c.charCodeAt(0));
                        const controller = new flash_controller_t(dev);
                        controller.flash_firmware(fw_bin);
                    };
                    reader.onerror = function (e) {
                        ;
                    };
                    reader.readAsBinaryString(resp);
                })
            }
        }
    }

