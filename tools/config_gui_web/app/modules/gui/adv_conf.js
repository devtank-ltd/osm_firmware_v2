import { disable_interaction } from './disable.js';
import { modbus_t } from './modbus.js';
import { current_clamp_t } from './current_clamp.js';
import { io_t } from './ios.js';
import { ftma_t } from './ftma.js';

export class adv_config_t {
  constructor(dev) {
    this.dev = dev;
    this.modbus = new modbus_t(this.dev);
    this.current_clamp = new current_clamp_t(this.dev);
    this.io = new io_t(this.dev);
    this.add_event_listeners = this.add_event_listeners.bind(this);
  }

  async open_adv_config_tab() {
    await disable_interaction(true);
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/adv_config.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    await this.add_event_listeners();
    await disable_interaction(false);
  }

  async add_event_listeners() {
    await this.modbus.open_modbus();

    await this.current_clamp.open_cc();

    await this.io.open_io();
  }
}
