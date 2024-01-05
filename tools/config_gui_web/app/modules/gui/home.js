import { navbar_t } from './navbar.js';
import { measurements_table_t } from './measurements_table.js';
import { lora_config_t } from './lora_config.js';
import { load_configuration_t } from './load_configuration.js';
import { lora_comms_t } from '../backend/binding.js';
import { console_t } from './console.js';

export class home_tab_t {
  constructor(dev) {
    this.dev = dev;
    this.change_to_console_tab = this.change_to_console_tab.bind(this);
    this.insert_homepage = this.insert_homepage.bind(this);
    this.return_to_home_tab = this.return_to_home_tab.bind(this);
  }

  async return_to_home_tab() {
    await this.insert_homepage();
    const disconnect = document.getElementById('home-disconnect');
    disconnect.addEventListener('click', () => {
      window.location.reload();
    });
  }

  async insert_homepage() {
    const doc = document.getElementById('main-page-body');
    const response = await fetch('modules/gui/html/home_page.html');
    const text = await response.text();
    doc.innerHTML = text;

    this.navbar = new navbar_t();
    await this.navbar.change_active_tab('home-tab');

    const meas_table = new measurements_table_t(this.dev);
    await meas_table.create_measurements_table_gui();
    await meas_table.add_uplink_listener();

    const comms_config = await this.dev.do_cmd('j_comms_cfg');
    const json_config = JSON.parse(comms_config);
    const comms_type = await json_config.type;
    if (comms_type === 'LW') {
      this.comms = new lora_comms_t(this.dev);
      const lora = new lora_config_t(this.comms);
      await lora.populate_lora_fields();
      await lora.add_listeners();
    }

    await this.load_serial_number();
    await this.load_fw_ver();

    const load_config = new load_configuration_t(this.dev);
    await load_config.add_listener();

    await this.add_event_listeners();
  }

  async load_serial_number() {
    this.sn = document.getElementById('home-serial-num');
    this.serial_num = await this.dev.serial_number;
    this.serial_num_join = `Serial Number: ${this.serial_num}`;
    this.sn.textContent = await this.serial_num_join;
  }

  async load_fw_ver() {
    this.fw = document.getElementById('home-firmware-version');
    this.fw_ver = await this.dev.firmware_version;
    this.split_fw = this.fw_ver.split('-');
    const [, num, hash] = this.split_fw;
    this.split_fw_join = `Firmware Version: ${num} - ${hash}`;
    this.fw.textContent = this.split_fw_join;
  }

  async add_event_listeners() {
    document.getElementById('console-tab').addEventListener('click', this.change_to_console_tab);
  }

  async change_to_console_tab() {
    this.console = new console_t(this.dev);
    await this.console.open_console();
    await this.navbar.change_active_tab('console-tab');
    document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
  }
}
