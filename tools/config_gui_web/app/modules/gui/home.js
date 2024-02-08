import { navbar_t } from './navbar.js';
import { measurements_table_t } from './measurements_table.js';
import { lora_config_t } from './lora_config.js';
import { load_configuration_t } from './load_configuration.js';
import { save_configuration_t } from './save_configuration.js';
import { lora_comms_t, wifi_comms_t } from '../backend/binding.js';
import { wifi_config_t } from './wifi_config.js';
import { console_t } from './console.js';
import { modbus_t } from './modbus.js';
import { current_clamp_t } from './current_clamp.js';
import { io_t } from './ios.js';
import { ftma_t } from './ftma.js';
import { disable_interaction } from './disable.js';

export class home_tab_t {
  constructor(dev) {
    this.dev = dev;
    this.navbar = new navbar_t();
    this.change_to_console_tab = this.change_to_console_tab.bind(this);
    this.insert_homepage = this.insert_homepage.bind(this);
    this.return_to_home_tab = this.return_to_home_tab.bind(this);
    this.save_settings = this.save_settings.bind(this);
    this.change_to_modbus_tab = this.change_to_modbus_tab.bind(this);
    this.change_to_current_clamp_tab = this.change_to_current_clamp_tab.bind(this);
    this.change_to_io_tab = this.change_to_io_tab.bind(this);
    this.change_to_ftma_tab = this.change_to_ftma_tab.bind(this);
  }

  async return_to_home_tab() {
    await disable_interaction(true);
    await this.insert_homepage();
    await disable_interaction(false);
  }

  async insert_homepage() {
    await disable_interaction(true);
    const body = document.getElementById('top-level-body');
    const doc = document.getElementById('main-page-body');
    const response = await fetch('modules/gui/html/home_page.html');
    const text = await response.text();
    doc.innerHTML = text;

    await this.navbar.insert_navbar();
    await this.navbar.change_active_tab('home-tab');

    const meas_table = new measurements_table_t(this.dev);
    await meas_table.create_measurements_table_gui();
    await meas_table.add_uplink_listener();

    const comms_type = await this.dev.comms_type();
    if (comms_type.includes('LW')) {
      this.comms = new lora_comms_t(this.dev);
      const lora = new lora_config_t(this.comms);
      await lora.populate_lora_fields();
      await lora.add_listeners();
    } else if (comms_type.includes('WIFI')) {
      this.comms = new wifi_comms_t(this.dev);
      const wifi = new wifi_config_t(this.comms);
      await wifi.populate_wifi_fields();
      await wifi.add_listeners();
    }

    await this.load_serial_number();
    await this.load_fw_ver();

    const load_config = new load_configuration_t(this.dev, this.comms);
    await load_config.add_listener();

    const save_config = new save_configuration_t(this.dev, this.comms);
    await save_config.add_event_listeners();

    await this.add_event_listeners();
    await disable_interaction(false);
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
    document.getElementById('global-save-setting').addEventListener('click', this.save_settings);
    document.getElementById('modbus-tab').addEventListener('click', this.change_to_modbus_tab);
    document.getElementById('current-clamp-tab').addEventListener('click', this.change_to_current_clamp_tab);
    document.getElementById('io-tab').addEventListener('click', this.change_to_io_tab);
  }

  async save_settings() {
    this.dev.save();
  }

  async change_to_console_tab() {
    this.console = new console_t(this.dev);
    await this.console.open_console();
    await this.navbar.change_active_tab('console-tab');
    document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
    document.getElementById('modbus-tab').addEventListener('click', this.change_to_modbus_tab);
    document.getElementById('current-clamp-tab').addEventListener('click', this.change_to_current_clamp_tab);
    document.getElementById('io-tab').addEventListener('click', this.change_to_io_tab);
  }

  async change_to_modbus_tab() {
    this.modbus = new modbus_t(this.dev);
    await this.modbus.open_modbus();
    await this.navbar.change_active_tab('modbus-tab');
    document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
    document.getElementById('console-tab').addEventListener('click', this.change_to_console_tab);
    document.getElementById('current-clamp-tab').addEventListener('click', this.change_to_current_clamp_tab);
    document.getElementById('io-tab').addEventListener('click', this.change_to_io_tab);
  }

  async change_to_current_clamp_tab() {
    this.cc = new current_clamp_t(this.dev);
    await this.cc.open_cc_tab();
    await this.navbar.change_active_tab('current-clamp-tab');
    document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
    document.getElementById('modbus-tab').addEventListener('click', this.change_to_modbus_tab);
    document.getElementById('console-tab').addEventListener('click', this.change_to_console_tab);
    document.getElementById('io-tab').addEventListener('click', this.change_to_io_tab);
  }

  async change_to_io_tab() {
    this.io = new io_t(this.dev);
    await this.io.open_io_tab();
    await this.navbar.change_active_tab('io-tab');
    document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
    document.getElementById('modbus-tab').addEventListener('click', this.change_to_modbus_tab);
    document.getElementById('console-tab').addEventListener('click', this.change_to_console_tab);
    document.getElementById('current-clamp-tab').addEventListener('click', this.change_to_current_clamp_tab);
  }

  async change_to_ftma_tab() {
    this.ftma = new ftma_t(this.dev);
    await this.ftma.open_ftma_tab();
    await this.navbar.change_active_tab('ftma-tab');
    document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
    document.getElementById('modbus-tab').addEventListener('click', this.change_to_modbus_tab);
    document.getElementById('console-tab').addEventListener('click', this.change_to_console_tab);
    document.getElementById('current-clamp-tab').addEventListener('click', this.change_to_current_clamp_tab);
    document.getElementById('io-tab').addEventListener('click', this.change_to_io_tab);
  }
}
