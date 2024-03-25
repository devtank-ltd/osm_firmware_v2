import { binding_t } from '../backend/binding.js';
import { home_tab_t } from './home.js';
import { disable_interaction } from './disable.js';

class config_gui_t {
    constructor() {
        this.spin_fake_osm = this.spin_fake_osm.bind(this);
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

                navigator.serial.addEventListener('connect', () => {

                });

                this.port.addEventListener('disconnect', () => {
                    this.port.close();
                    this.disconnect_modal();
                });
                const loader = document.getElementById('loader');
                loader.style.display = 'block';
                this.dev = new binding_t(this.port, type);
                this.writer = await this.dev.open_ll_obj();
                if (this.writer) {
                    this.home = new home_tab_t(this.dev, false);
                    await this.home.create_navbar();
                    await this.home.insert_homepage();
                    const disconnect = document.getElementById('global-disconnect');
                    disconnect.addEventListener('click', () => {
                        this.port.close();
                        this.port = null;
                        window.location.reload();
                    });
                    const globalbtns = document.getElementById('global-load-save-config-buttons');
                    globalbtns.style.removeProperty('display');
                } else {
                    const error_div = document.getElementById('error-div');
                    error_div.textContent = 'Failed to connect, refresh page and try again.';
                }
            })
            .catch((e) => {
                console.log(e);
            });
    }

    async disconnect_modal() {
        this.dialog = document.getElementById('osm-disconnect-dialog');
        this.confirm = document.getElementById('osm-disconnect-confirm');
        this.dialog.showModal();
        const controller = new AbortController();

        this.confirm.addEventListener('click', async () => {
            this.dialog.close();
            controller.abort();
            window.location.reload();
        });
    }

    async add_event_listeners() {
        const conn = document.getElementById('main-page-connect');
        if (!navigator.serial) {
            conn.style.backgroundColor = 'grey';
            conn.disabled = true;
            conn.title = 'Not supported on this browser yet. Please use Google Chrome to access this feature.';
        }
        conn.addEventListener('click', this.open_serial);
        document.getElementById('main-page-websocket-connect').addEventListener('click', this.spin_fake_osm);
    }

    async spin_fake_osm() {
        await disable_interaction(true);
        const error_div = document.getElementById('error-div');
        const loader = document.getElementById('loader');
        this.url = `${window.document.URL}websocket`;
        const xmlHttp = new XMLHttpRequest();
        xmlHttp.open('GET', this.url, true);
        this.dev = new binding_t(this.url, 'websocket');
        loader.style.display = 'block';
        this.writer = await this.dev.open_ll_obj();
        if (this.writer) {
            this.home = new home_tab_t(this.dev, true);
            await this.home.create_navbar();
            await this.home.insert_homepage();

            const disconnect = document.getElementById('global-disconnect');
            disconnect.addEventListener('click', async () => {
                await this.dev.ll.write('close');
                await this.dev.ll.url.close();
                window.location.reload();
            });

            const globalbtns = document.getElementById('global-load-save-config-buttons');
            globalbtns.style.removeProperty('display');
        } else {
            error_div.textContent = 'Failed to connect, refresh page and try again.';
            loader.style.display = 'none';
        }
    }
}

const gui = new config_gui_t();
