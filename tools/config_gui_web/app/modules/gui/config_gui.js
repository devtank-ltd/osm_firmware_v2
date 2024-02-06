import { binding_t } from '../backend/binding.js';
import { home_tab_t } from './home.js';

class config_gui_t {
  constructor() {
    this.add_event_listeners();
  }

  async open_serial() {
    const filter = { usbVendorId: '0x10c4' };
    const type = 'Serial';
    let port = await navigator.serial.requestPort({ filters: [filter] });
    port.getInfo();

    await port.open({
      baudRate: 115200, databits: 8, stopbits: 1, parity: 'none',
    });
    console.log('User connected to device ', port);

    navigator.serial.addEventListener('connect', () => {
      console.log('USB device available.');
    });

    navigator.serial.addEventListener('disconnect', () => {
      console.log('USB device disconnect detected.');
    });

    this.dev = new binding_t(port, type);
    this.home = new home_tab_t(this.dev);
    await this.home.insert_homepage();
    const disconnect = document.getElementById('global-disconnect');
    disconnect.addEventListener('click', () => {
      port.close();
      port = null;
      console.log('Disconnected');
      window.location.reload();
    });
    const globalbtns = document.getElementById('global-load-save-config-buttons');
    globalbtns.style.removeProperty('display');
  }

  async add_event_listeners() {
    document.getElementById('main-page-connect').addEventListener('click', this.open_serial);
    document.getElementById('main-page-websocket-connect').addEventListener('click', this.spin_fake_osm);
  }

  async spin_fake_osm() {
    this.msg = 'Spawn virtual OSM';
    this.body = JSON.stringify({
      cmd: this.msg,
    });
    const request = new XMLHttpRequest();
    request.open('POST', 'http://localhost:8000', false); // `false` makes the request synchronous
    request.send(this.body);

    if (request.status === 200) {
      this.resp = JSON.parse(request.responseText);
      this.url = await this.resp.websocket;
      this.port_check = new WebSocket(this.url);
      this.port_check.onopen = async (e) => {
        console.log(`Socket detected at ${this.url}`);
        this.port_check.close();
        this.dev = new binding_t(this.url, 'websocket');
        this.home = new home_tab_t(this.dev);
        await this.home.insert_homepage();
        const disconnect = document.getElementById('global-disconnect');
        disconnect.addEventListener('click', () => {
          console.log('Disconnected');
          window.location.reload();
        });
        const globalbtns = document.getElementById('global-load-save-config-buttons');
        globalbtns.style.removeProperty('display');
      };
      this.port_check.onerror = (event) => {
        console.log('WebSocket error: ', event);
        const div = document.createElement('div');
        const main = document.getElementById('main-page-body');
        main.appendChild(div);
        div.textContent = 'No Virtual OSM detected.';
      };
    }
  }
}

const gui = new config_gui_t();
