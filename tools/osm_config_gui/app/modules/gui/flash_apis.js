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

export class rak3172_flash_api_t extends STMApi {
    constructor(port, params) {
        super(port);
        this.dev = params.dev;
        this.open_params;
        this.dev_params = {
                baudRate: 115200, databits: 8, stopbits: 1, parity: 'none',
            };
    }

    /**
     * Connect to the target by resetting it and activating the ROM bootloader
     * @param {object} params
     * @returns {Promise}
     */
    async connect(params) {
        const open_params = {
            baudRate: parseInt(params.baudrate, 10),
            parity: this.replyMode ? 'none' : 'even',
        };
        this.open_params = open_params;
        console.log('CONNECT');
        this.ewrLoadState = EwrLoadState.NOT_LOADED;
        return new Promise((resolve, reject) => {
            this.activateBootloader()
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
        let dev_params = this.dev_params;
        console.log('DISCONNECT');
        return new Promise((resolve, reject) => {
            setTimeout(() => {
                this.serial.close()
                    .then(() => this.dev.ll.port.open(dev_params))
                    .then(() => this.dev.do_cmd_multi(''))
                    .then(() => this.dev.do_cmd_multi('comms_boot 0'))
                    .then(() => this.resetTarget())
                    .then(resolve)
                    .catch(reject);
            }, 4000);
        });
    }

    /**
     * Activate the ROM bootloader
     * @private
     * @returns {Promise}
     */
    async activateBootloader() {
        let open_params = this.open_params;
        console.log("open_params = ", open_params);
        console.log('BOOTLOADER');
        return new Promise((resolve, reject) => {
            this.dev.do_cmd_multi('')
                .then(() => this.dev.do_cmd_multi('comms_boot 1'))
                .then(() => {
                    console.log('HACK 0');
                    return this.dev.do_cmd_multi('comms_reset 0');
                })
                .then(() => {
                    console.log('HACK 1');
                    return this.dev.do_cmd_multi('comms_reset 1');
                })
                .then(() => {
                    console.log('HACK 2');
                    return this.dev.ll.write('comms_direct\r\n');
                })
                .then(() => {
                    console.log('HACK 2.1');
                    return this.dev.ll.read_raw();
                })
                .then((response) => {
                    console.log('HACK 2.2');
                    console.log('response = ', response);
                })
                .then(() => {
                    return this.dev.ll.port.close();
                })
                .then(() => {
                    return this.serial.open(open_params);
                })
                .then(() => sleep(100)) /* Wait for bootloader to finish booting */
                .then(() => {
                    console.log('HACK 3');
                    this.serial.write(u8a([SYNCHR]));
                })
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
        console.log('RESET');
        return new Promise((resolve, reject) => {
            this.dev.do_cmd_multi('comms_reset 0')
                .then(() => this.dev.do_cmd_multi('comms_reset 1'))
                .then(() => {
                    resolve();
                })
                .catch(reject);
        });
    }
}
