import WebSerial from '../stm-serial-flasher/src/api/WebSerial.js';
import settings from '../stm-serial-flasher/src/api/Settings.js';
import { osm_flash_api_t } from './flash_apis.js';


class flash_controller_base_t {
    constructor(params) {
        this.dev            = params.dev;
        this.get_url        = params.get_url;
        this.get_info_url   = params.get_info_url;
        this.api_type       = params.api_type;
        this.baudrate       = params.baudrate;
        this.flash_start    = this.flash_start.bind(this);
        this.get_records    = this.get_records.bind(this);
    }

    get_latest_firmware_info_loaded(evt) {
        let firmware_info = JSON.parse(evt.target.response);
        console.log("FIRMWARE INFO: LOADED:", firmware_info);
    }

    get_latest_firmware_info_error(evt) {
        console.log("FIRMWARE INFO: ERROR");
        console.log(evt);
    }

    async get_latest_firmware_info() {
        const req = new XMLHttpRequest();
        req.addEventListener("load", (e) => { this.get_latest_firmware_info_loaded(e) });
        req.addEventListener("error", (e) => { this.get_latest_firmware_info_error(e) });
        req.open("GET", "/latest_firmware_info");
        req.overrideMimeType("application/json");
        req.send();
    }

    get_records(data) {
        throw {name : "NotImplementedError", message : "Not implemented"};
    }

    async write_data(api, records) {
        return new Promise( async (resolve, reject) => {
            for (let i = 0; i < records.length; i++) {
                let rec = records[i];
                console.log("ADDRESS: 0x" + rec.address.toString(16));
                console.log("SIZE = " + rec.data.length);
                await api.write(rec.data, rec.address);
                console.log("WRITTEN");
            }
            console.log("FINISHED WRITING");
            resolve();
        });
    }

    flash_start (stm_api) {
        let baudrate = this.baudrate;
        return new Promise((resolve, reject) => {
            let deviceInfo = {
                family: '-',
                bl: '-',
                pid: '-',
                commands: [],
            };

            let error = null;
            console.log("BAUD:", baudrate);
            stm_api.connect({baudrate: baudrate, replyMode: false})
                .then(() => {return stm_api.cmdGET()})
                .then((info) => {
                    deviceInfo.bl = info.blVersion;
                    deviceInfo.commands = info.commands;
                    deviceInfo.family = info.getFamily();
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

    flash_firmware_loaded(evt) {
        let port        = this.dev.port;
        let api_type    = this.api_type;
        let flash_start = this.flash_start;
        let get_records = this.get_records;
        let write_data  = this.write_data;
        let baudrate    = this.baudrate;
        return new Promise((resolve, reject) => {
            console.log("FIRMWARE: LOADED");
            let fw_b64 = evt.target.response;
            let data = Uint8Array.from(atob(fw_b64), c => c.charCodeAt(0))

            let stm_api;
            let serial;
            port.close()
                .then(() => {
                    serial = new WebSerial(port)
                    serial.onConnect = () => {};
                    serial.onDisconnect = () => {};
                })
                .then(() => {
                    new Promise((resolve, reject) => {
                        stm_api = new api_type(serial);
                        resolve();
                        });
                    })
                .then(() => flash_start(stm_api))
                .then(() => console.log("FINISHED START"))
                .then(() => stm_api.eraseAll())
                .then(() => {
                    let records = get_records(data);
                    return write_data(stm_api, records);
                })
                .then(() => {
                    console.log("GONE");
                    return stm_api.disconnect();
                })
                .then(() => port.open({"baudRate": baudrate}))
                .then(resolve)
                .catch(reject);
        });
    }

    flash_firmware_error(evt) {
        throw {name : "DownloadError", message : "Firmware failed to download"};
    }

    async flash_firmware() {
        if (confirm("Are you sure you want to update the firmware?")) {
            alert("Do not unplug the device while programming");
            const req = new XMLHttpRequest();
            req.addEventListener("load", (e) => { this.flash_firmware_loaded(e) });
            req.addEventListener("error", (e) => { this.flash_firmware_error(e) });
            req.open("GET", this.get_url);
            req.overrideMimeType("application/octet-stream");
            req.send();
        }
    }
}


class flash_controller_t extends flash_controller_base_t {
    constructor(dev) {
        super({
            dev             : dev,
            get_url         : "/latest_firmware",
            get_info_url    : "/latest_firmware_info",
            api_type        : osm_flash_api_t,
            baudrate        : 115200
            });
        this.PAGE_SIZE          = 0x800;
        this.SIZE_BOOTLOADER    = 2 * this.PAGE_SIZE;
        this.SIZE_CONFIG        = 2 * this.PAGE_SIZE;
        this.ADDRESS_BOOTLOADER = parseInt(settings.startAddress);
        this.ADDRESS_CONFIG     = this.ADDRESS_BOOTLOADER + this.SIZE_BOOTLOADER
        this.ADDRESS_FIRMWARE   = this.ADDRESS_CONFIG + this.SIZE_CONFIG;
    }

    get_records(data) {
        let records = [{
                data: new Uint8Array(data.slice(0, this.SIZE_BOOTLOADER)),
                address: this.ADDRESS_BOOTLOADER
            },
            {
                data: new Uint8Array(data.slice(this.ADDRESS_FIRMWARE - this.ADDRESS_BOOTLOADER, -1)),
                address: this.ADDRESS_FIRMWARE
            }]
        return records;
    }
}


export function flash_firmware(dev) {
    return new Promise((resolve, reject) => {
        let flash_controller = new flash_controller_t(dev);
        flash_controller.flash_firmware()
            .then(resolve)
            .catch(reject);
    });
}

