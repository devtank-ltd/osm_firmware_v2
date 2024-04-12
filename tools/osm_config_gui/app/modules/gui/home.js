import { navbar_t } from './navbar.js';
import { measurements_table_t } from './measurements_table.js';
import { lora_config_t } from './lora_config.js';
import { load_configuration_t } from './load_configuration.js';
import { save_configuration_t } from './download_config.js';
import { lora_comms_t, wifi_comms_t } from '../backend/binding.js';
import { wifi_config_t } from './wifi_config.js';
import { console_t } from './console.js';
import { adv_config_t } from './adv_conf.js';
import { disable_interaction, limit_characters } from './disable.js';
import { firmware_t, rak3172_firmware_t } from './flash.js';

export class home_tab_t {
    constructor(dev, is_virtual) {
        this.dev = dev;
        this.is_virtual = is_virtual;
        this.create_navbar = this.create_navbar.bind(this);
        this.change_serial_num = this.change_serial_num.bind(this);
        this.change_to_adv_conf_tab = this.change_to_adv_conf_tab.bind(this);
        this.change_to_console_tab = this.change_to_console_tab.bind(this);
        this.insert_homepage = this.insert_homepage.bind(this);
        this.return_to_home_tab = this.return_to_home_tab.bind(this);
        this.save_settings = this.save_settings.bind(this);
        this.hide_img();
    }

    async hide_img() {
        this.hide = document.getElementById('gui-img');
        this.hide.style.display = 'none';
    }

    async return_to_home_tab() {
        await disable_interaction(true);
        await this.insert_homepage();
        await disable_interaction(false);
    }

    async create_navbar() {
        this.navbar = new navbar_t();
        await this.navbar.insert_navbar();
        return this.navbar;
    }

    async insert_homepage() {
        const loader = document.getElementById('loader');
        loader.style.display = 'block';
        await disable_interaction(true);
        const doc = document.getElementById('main-page-body');
        const response = await fetch('modules/gui/html/home_page.html');
        const text = await response.text();
        doc.innerHTML = text;

        await this.dev.do_cmd('?');

        await this.navbar.change_active_tab('home-tab');

        const meas_table = new measurements_table_t(this.dev);
        await meas_table.create_measurements_table_gui();
        await meas_table.add_uplink_listener();

        await this.load_fw_ver();

        if (!this.is_virtual) {
            const fw_table = new firmware_t(this.dev);
            const model = this.fw_ver.split('-[')[0];
            fw_table.get_latest_firmware_info(model);
            const comms_fw = new rak3172_firmware_t(this.dev);
            comms_fw.add_comms_btn_listener();
        }

        const comms_type = await this.dev.comms_type();
        if (comms_type.includes('LW')) {
            this.comms = new lora_comms_t(this.dev);
            const lora = new lora_config_t(this.comms);
            await lora.populate_lora_fields();
            await lora.add_listeners();
            const fw_table = document.getElementById('home-firmware-div');
            fw_table.style.gridColumnStart = 2;
            fw_table.style.gridColumnEnd = 3;
        } else if (comms_type.includes('WIFI')) {
            this.comms = new wifi_comms_t(this.dev);
            const wifi = new wifi_config_t(this.comms);
            await wifi.populate_wifi_fields();
            await wifi.add_listeners();
        }

        await this.load_serial_number();

        const load_config = new load_configuration_t(this.dev, this.comms);
        await load_config.add_listener();

        const save_config = new save_configuration_t(this.dev, this.comms);
        await save_config.add_event_listeners();

        await this.add_event_listeners();
        await disable_interaction(false);
        loader.style.display = 'none';
    }

    async load_serial_number() {
        this.sn_input = document.getElementById('serial-num-input');
        this.serial_num = await this.dev.serial_number;
        this.sn_input.value = this.serial_num;
        this.sn_input.contentEditable = true;
        this.sn_input.oninput = (e) => { limit_characters(e, 20); };
        this.sn_input.addEventListener('focusout', this.change_serial_num);
    }

    async change_serial_num(e) {
        this.val = e.target.value;
        this.dev.serial_number = this.val;
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
        document.getElementById('adv-conf-tab').addEventListener('click', this.change_to_adv_conf_tab);
    }

    async save_settings() {
        const savemsg = document.getElementById('global-save-config-msg');
        this.dev.save();
        savemsg.textContent = 'Saved';
    }

    async change_to_adv_conf_tab() {
        this.adv_conf = new adv_config_t(this.dev);
        await this.adv_conf.open_adv_config_tab();
        await this.navbar.change_active_tab('adv-conf-tab');
        document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
        document.getElementById('console-tab').addEventListener('click', this.change_to_console_tab);
    }

    async change_to_console_tab() {
        this.console = new console_t(this.dev);
        await this.console.open_console();
        await this.navbar.change_active_tab('console-tab');
        document.getElementById('home-tab').addEventListener('click', this.return_to_home_tab);
        document.getElementById('adv-conf-tab').addEventListener('click', this.change_to_adv_conf_tab);
    }
}
