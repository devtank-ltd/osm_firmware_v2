import { navbar_t } from './navbar.js';
import { measurements_table_t } from './measurements_table.js';
import { lora_config_t } from './lora_config.js';
import { load_configuration_t } from './load_configuration.js';
import { lora_comms_t } from '../backend/binding.js';
import { home_page } from './html/home_page_html.js';

export class home_tab_t {
  constructor(dev) {
    this.dev = dev;
  }

  async insert_homepage() {
    const doc = document.getElementById('main-page-body');
    doc.innerHTML = home_page;

    const active_tab = new navbar_t('home');
    active_tab.change_active_tab();

    const meas_table = new measurements_table_t(this.dev);
    await meas_table.create_measurements_table_gui();
    await meas_table.add_uplink_listener();

    const comms_config = await this.dev.do_cmd('comms_pr_cfg');
    const json_config = JSON.parse(comms_config);
    const comms_type = await json_config.type;
    if (comms_type === 'LW PENGUIN') {
      this.comms = new lora_comms_t(this.dev);
      const lora = new lora_config_t(this.comms);
      await lora.populate_lora_fields();
      await lora.add_listeners();
    }

    await this.load_serial_number();
    await this.load_fw_ver();

    const load_config = new load_configuration_t(this.dev);
    await load_config.add_listener();
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
}
