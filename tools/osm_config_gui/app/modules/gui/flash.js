import { STMApi } from '../stm-serial-flasher/src/api/STMapi.js'
import WebSerial from '../stm-serial-flasher/src/api/WebSerial.js';
import settings from '../stm-serial-flasher/src/api/Settings.js';

import { disable_interaction } from './disable.js';


const PIN_HIGH = false;
const PIN_LOW = true;

const SYNCHR = 0x7F;
const ACK = 0x79;
const NACK = 0x1F;

function u8a(array) {
    return new Uint8Array(array);
}


const sleep = ms => new Promise(r => setTimeout(r, ms));


const EwrLoadState = Object.freeze({
    NOT_LOADED: Symbol("not_loaded"),
    LOADING: Symbol("loading"),
    LOADED: Symbol("loaded")
});


class osm_flash_api_t extends STMApi {
    /**
     * Connect to the target by resetting it and activating the ROM bootloader
     * @param {object} params
     * @returns {Promise}
     */
    async connect(params) {
        this.ewrLoadState = EwrLoadState.NOT_LOADED;
        return new Promise((resolve, reject) => {
            console.log('Connecting with baudrate ' + params.baudrate + ' and reply mode ' + (params.replyMode ? 'on' : 'off'));
            if (this.serial.isOpen()) {
                reject(new Error('Port already opened'));
                return;
            }

            this.replyMode = params.replyMode || false;

            let open_params = {
                baudRate: parseInt(params.baudrate, 10),
                parity: this.replyMode ? 'none' : 'even'
            }
            let signal = {};
            this.serial.open(open_params)
                .then(() => {
                    signal["dataTerminalReady"] = PIN_HIGH;
                    signal["requestToSend"] = PIN_HIGH;
                    return this.serial.control(signal)
                })
                .then(() => console.log("Before BOOTLOADER", Date.now()))
                .then(() => this.activateBootloader())
                .then(resolve)
                .catch(reject);
        });
    }

    /**
     * Close current connection. Before closing serial connection disable bootloader and reset target
     * @returns {Promise}
     */
    async disconnect() {
        return new Promise((resolve, reject) => {
            let signal = {}
            signal["dataTerminalReady"] = PIN_HIGH;
            signal["requestToSend"] = PIN_HIGH;
            this.serial.control(signal)
                .then(() => this.resetTarget())
                .then(() => this.serial.close())
                .then(resolve)
                .catch(reject);
        });

    }

    /**
     * Activate the ROM bootloader
     * @private
     * @returns {Promise}
     */
    async activateBootloader() {
        return new Promise((resolve, reject) => {
            console.log('Activating bootloader...');
            if (!this.serial.isOpen()) {
                reject(new Error('Port must be opened before activating the bootloader'));
                return;
            }

            let signal = {};
            signal["dataTerminalReady"] = PIN_LOW;
            signal["requestToSend"] = PIN_HIGH;
            this.serial.control(signal)
                .then(() => {
                    signal["dataTerminalReady"] = PIN_LOW;
                    signal["requestToSend"] = PIN_LOW;
                    this.serial.control(signal);
                })
                .then(() => sleep(100)) /* Wait for bootloader to finish booting */
                .then(() => this.serial.write(u8a([SYNCHR])))
                .then(() => this.serial.read())
                .then(response => {
                    if (response[0] === ACK) {
                        if (this.replyMode) {
                            return this.serial.write(u8a([ACK]));
                        }
                        return Promise.resolve();
                    } else {
                        throw new Error('Unexpected response');
                    }
                })
                .then(() => {
                    console.log('Bootloader is ready for commands');
                    resolve();
                })
                .catch(reject);
        });
    }

    /**
     * Resets the target by toggling a control pin defined in RESET_PIN
     * @private
     * @returns {Promise}
     */
    async resetTarget() {
        return new Promise((resolve, reject) => {
            console.log('Resetting target...');
            let signal = {};

            if (!this.serial.isOpen()) {
                reject(new Error('Port must be opened for device reset'));
                return;
            }

            signal["dataTerminalReady"] = PIN_HIGH;
            signal["requestToSend"] = PIN_LOW;
            this.serial.control(signal)
                .then(() => {
                    signal["dataTerminalReady"] = PIN_HIGH;
                    signal["requestToSend"] = PIN_HIGH;
                    return this.serial.control(signal);
                })
                .then(() => {
                    // wait for device init
                    console.log('Reset done. Wait for init.');
                    this.ewrLoadState = EwrLoadState.NOT_LOADED;
                    setTimeout(resolve, 200);
                })
                .catch(reject);
        });
    }
}


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

    flash_firmware(fw_b64) {
        const loader = document.getElementById('loader');
        loader.style.display = 'block';
        let disabled = disable_interaction(true);
        if (disabled) {
            console.log("FIRMWARE: LOADED");
            let data = Uint8Array.from(atob(fw_b64), c => c.charCodeAt(0))

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
                .then(() => console.log("FINISHED START"))
                .then(() => osmAPI.eraseAll() )
                .then(async () => {
                    console.log("BOOTLOADER ADDRESS: 0x" + this.ADDRESS_BOOTLOADER.toString(16));
                    let data_bootloader = new Uint8Array(data.slice(0, this.SIZE_BOOTLOADER));
                    console.log("BOOTLOADER SIZE = " + data_bootloader.length);
                    await osmAPI.write(data_bootloader, this.ADDRESS_BOOTLOADER);

                    console.log("FIRMWARE ADDRESS: 0x" + this.ADDRESS_FIRMWARE.toString(16));
                    let data_firmware = new Uint8Array(data.slice(this.ADDRESS_FIRMWARE - this.ADDRESS_BOOTLOADER, -1));
                    console.log("FIRMWARE SIZE = " + data_firmware.length);
                    await osmAPI.write(data_firmware, this.ADDRESS_FIRMWARE);
                    console.log("FINISHED WRITING");
                })
                .then(() => {
                    console.log("GONE");
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

        const flash_btn = document.getElementById('fw-btn');
        flash_btn.style.display = 'block';
        flash_btn.addEventListener('click', () => { this.flash_latest(fw_info) });

        await disable_interaction(false);
    }

    get_latest_firmware_info() {
        fetch('../../fw_releases/latest_fw_info.json')
            .then((resp) => resp.json())
            .then((json) => {
                console.log("FIRMWARE INFO: LOADED:", json);
                this.create_firmware_table(json);
            })
            .catch((err) => {
                console.log("FIRMWARE INFO: ERROR");
                console.log(err);
            })
    }

    flash_latest(fw_info) {
        const dev = this.dev;
        if (confirm("Are you sure you want to update the firmware?")) {
            console.log("FIRMWARE: CONFIRMED");
            const fw_path = fw_info.path;
            fetch(`../../${fw_path}`)
                .then((r) => r.blob())
                .then((resp) => {
                    console.log(resp);
                    var reader = new FileReader();
                    reader.onload = function (e) {
                        const fw_b64 = btoa(e.target.result);
                        const controller = new flash_controller_t(dev);
                        controller.flash_firmware(fw_b64);
                    };
                    reader.onerror = function (e) {
                        console.log("FIRMWARE: ERROR");
                    };
                    reader.readAsBinaryString(resp);
                })
            }
        }
    }

