import { binding_t } from '../backend/binding.js';
import { home_tab_t } from './home.js';
import { disable_interaction } from './disable.js';
import { osm_flash_api_t } from './flash.js';

class config_gui_t {
  constructor() {
    this.add_event_listeners();
    this.server_check();
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

        navigator.serial.addEventListener('disconnect', () => {
          console.log('USB device disconnect detected.');
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

  async add_event_listeners() {
    document.getElementById('main-page-connect').addEventListener('click', this.open_serial);
    document.getElementById('main-page-websocket-connect').addEventListener('click', this.spin_fake_osm);
  }

  async server_check() {
    const web = document.getElementById('main-page-websocket-connect');
    const con = document.getElementById('main-page-connect')
    await fetch('http://localhost:8000', {}).catch(() => {
      web.style.display = 'none';
      con.innerHTML = 'Connect';
    })
  }

  async spin_fake_osm() {
    await disable_interaction(true);
    this.msg = 'Spawn virtual OSM';
    const error_div = document.getElementById('error-div');
    this.body = JSON.stringify({
      cmd: this.msg,
    });
    fetch('http://localhost:8000', {
      method: 'POST',
      body: this.body,
      headers: {
        'Content-type': 'application/json; charset=UTF-8',
      }
    }).then(async (response) => {
      this.resp = await response.json();
      this.url = await this.resp.websocket;
      this.given_port = await this.resp.port;
      this.location = await this.resp.location;
      this.port_check = new WebSocket(this.url);
      this.port_check.onopen = async (e) => {
        console.log(`Socket detected at ${this.url}`);
        this.port_check.close();

        this.dev = new binding_t(this.url, 'websocket');
        this.home = new home_tab_t(this.dev);
        await this.home.insert_homepage();

        window.addEventListener('beforeunload', (event) => {
          this.close = JSON.stringify({
            cmd: 'Close',
            port: this.given_port,
            location: this.location,
          });
          fetch('http://localhost:8000', {
            method: 'POST',
            body: this.close,
            headers: {
              'Content-type': 'application/json; charset=UTF-8',
            },
          }).then(document.getElementById('main-page-body').innerHTML = event);
        });

        const disconnect = document.getElementById('global-disconnect');
        disconnect.addEventListener('click', () => {
          console.log('Disconnected');
          this.close = JSON.stringify({
            cmd: 'Close',
            port: this.given_port,
            location: this.location,
          });
          fetch('http://localhost:8000', {
            method: 'POST',
            body: this.close,
            headers: {
              'Content-type': 'application/json; charset=UTF-8',
            },
          }).then((resp) => resp.json())
            .then(() => {
              window.location.reload();
            });
        });

        const globalbtns = document.getElementById('global-load-save-config-buttons');
        globalbtns.style.removeProperty('display');
      };
      this.port_check.onerror = (event) => {
        console.log('WebSocket error: ', event);
        error_div.textContent = 'No Virtual OSM detected.';
      };
    });
  }
}

const gui = new config_gui_t();
