import { binding_t } from '../backend/binding.js';
import { home_tab_t } from './home.js';
import { disable_interaction } from './disable.js';
import { osm_flash_api_t } from './flash.js';

class config_gui_t {
    constructor() {
        this.add_event_listeners = this.add_event_listeners.bind(this);
        this.open_serial = this.open_serial.bind(this);
        this.add_event_listeners();
    }

    async open_serial() {
        const filter = { usbVendorId: '0x10c4' };
        const type = 'Serial';
        navigator.serial
            .requestPort({ filters: [filter] })
            .then(async (port) => {
                this.port = port;
                this.port.getInfo();

                this.port.open({
                    baudRate: 115200, databits: 8, stopbits: 1, parity: 'none',
                });
                console.log('User connected to device ', this.port);

                navigator.serial.addEventListener('connect', () => {
                    console.log('USB device available.');
                });

                this.port.addEventListener('disconnect', () => {
                    console.log('USB device disconnect detected.');
                    this.port.close();
                    this.disconnect_modal();
                });

                this.dev = new binding_t(this.port, type);
                this.home = new home_tab_t(this.dev);
                await this.home.insert_homepage();
                const disconnect = document.getElementById('global-disconnect');
                disconnect.addEventListener('click', () => {
                    this.port.close();
                    this.port = null;
                    console.log('Disconnected');
                    window.location.reload();
                });
                const globalbtns = document.getElementById('global-load-save-config-buttons');
                globalbtns.style.removeProperty('display');
            })
            .catch((e) => {
                console.log(e);
            });
    }

    async disconnect_modal() {
        this.dialog = document.getElementById('usb-disconnect-dialog');
        this.confirm = document.getElementById('usb-disconnect-confirm')
        this.dialog.showModal();
        const controller = new AbortController();

        this.confirm.addEventListener('click', async () => {
            this.dialog.close();
            controller.abort();
            window.location.reload();
        });
    }

    async add_event_listeners() {
        document.getElementById('main-page-connect').addEventListener('click', this.open_serial);
        document.getElementById('main-page-websocket-connect').addEventListener('click', this.spin_fake_osm);
    }

    async spin_fake_osm() {
        await disable_interaction(true);
        const error_div = document.getElementById('error-div');
        this.url = window.document.URL + 'api';
        const headers = new Headers();
        fetch(this.url, {
            method: "GET",
            headers: headers,
            mode: "cors",
            cache: "no-cache",
        }).then(async (resp) => {
            console.log(resp);
            this.dev = new binding_t(this.url, 'websocket');
            this.home = new home_tab_t(this.dev);
            await this.home.insert_homepage();

            const disconnect = document.getElementById('global-disconnect');
            disconnect.addEventListener('click', () => {
                this.dev.ll.write('close');
                window.location.reload(true);
            });

            const globalbtns = document.getElementById('global-load-save-config-buttons');
            globalbtns.style.removeProperty('display');
        }).catch((error) => {
            error_div.textContent = 'No Virtual OSM detected.'
        });
    }
};

const gui = new config_gui_t();
