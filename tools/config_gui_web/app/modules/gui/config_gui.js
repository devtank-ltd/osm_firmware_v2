import { binding_t } from '../backend/binding.js';
import { home_tab_t } from './home.js';

class config_gui_t {
  constructor() {
    this.listen_open_serial();
  }

  async open_serial() {
    const filter = { usbVendorId: '0x10c4' };
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

    this.dev = new binding_t(port);
    this.home = new home_tab_t(this.dev);
    await this.home.insert_homepage();
    const disconnect = document.getElementById('home-disconnect');
    disconnect.addEventListener('click', () => {
      port.close();
      port = null;
      console.log('Disconnected');
      const config_gui = `<h1 id="main-page-title">OpenSmartMonitor Configuration</h1>
                              <button id='main-page-connect'>Connect</button>`;
      document.getElementById('main-page-body').innerHTML = config_gui;
    });
  }

  async listen_open_serial() {
    document.getElementById('main-page-connect').addEventListener('click', this.open_serial);
  }
}

const gui = new config_gui_t();
