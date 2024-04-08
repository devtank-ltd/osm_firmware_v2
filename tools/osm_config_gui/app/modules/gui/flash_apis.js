import { STMApi } from '../../libs/stm-serial-flasher/api/STMapi.js';
import logger from '../../libs/stm-serial-flasher/api/Logger.js';

const PIN_HIGH = false;
const PIN_LOW = true;

const SYNCHR = 0x7F;
const ACK = 0x79;
const NACK = 0x1F;

function u8a(array) {
    return new Uint8Array(array);
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const EwrLoadState = Object.freeze({
    NOT_LOADED: Symbol('not_loaded'),
    LOADING: Symbol('loading'),
    LOADED: Symbol('loaded'),
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
            if (this.serial.isOpen()) {
                reject(new Error('Port already opened'));
                return;
            }

            this.replyMode = params.replyMode || false;

            const open_params = {
                baudRate: parseInt(params.baudrate, 10),
                parity: this.replyMode ? 'none' : 'even',
            };
            const signal = {};
            this.serial.open(open_params)
                .then(() => {
                    signal.dataTerminalReady = PIN_HIGH;
                    signal.requestToSend = PIN_HIGH;
                    return this.serial.control(signal);
                })
                .then(() => this.activateBootloader())
                .then(resolve)
                .catch(reject);
        });
    }

    /**
     * Close current connection. Before closing serial connection
     * disable bootloader and reset target
     * @returns {Promise}
     */
    async disconnect() {
        return new Promise((resolve, reject) => {
            const signal = {};
            signal.dataTerminalReady = PIN_HIGH;
            signal.requestToSend = PIN_HIGH;
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
            if (!this.serial.isOpen()) {
                reject(new Error('Port must be opened before activating the bootloader'));
                return;
            }

            const signal = {};
            signal.dataTerminalReady = PIN_LOW;
            signal.requestToSend = PIN_HIGH;
            this.serial.control(signal)
                .then(() => {
                    signal.dataTerminalReady = PIN_LOW;
                    signal.requestToSend = PIN_LOW;
                    this.serial.control(signal);
                })
                .then(() => sleep(100)) /* Wait for bootloader to finish booting */
                .then(() => this.serial.write(u8a([SYNCHR])))
                .then(() => this.serial.read())
                .then((response) => {
                    if (response[0] === ACK) {
                        if (this.replyMode) {
                            return this.serial.write(u8a([ACK]));
                        }
                        return Promise.resolve();
                    }
                    throw new Error('Unexpected response');
                })
                .then(() => {
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
            const signal = {};

            if (!this.serial.isOpen()) {
                reject(new Error('Port must be opened for device reset'));
                return;
            }

            signal.dataTerminalReady = PIN_HIGH;
            signal.requestToSend = PIN_LOW;
            this.serial.control(signal)
                .then(() => {
                    signal.dataTerminalReady = PIN_HIGH;
                    signal.requestToSend = PIN_HIGH;
                    return this.serial.control(signal);
                })
                .then(() => {
                    // wait for device init
                    this.ewrLoadState = EwrLoadState.NOT_LOADED;
                    setTimeout(resolve, 200);
                })
                .catch(reject);
        });
    }
}
