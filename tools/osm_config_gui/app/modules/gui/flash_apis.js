import { STMApi } from '../stm-serial-flasher/src/api/STMapi.js'
import logger from '../stm-serial-flasher/src/api/Logger.js';


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


export class osm_flash_api_t extends STMApi {
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
