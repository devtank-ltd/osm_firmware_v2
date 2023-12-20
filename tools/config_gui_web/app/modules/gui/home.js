import { navbar_t } from './navbar.js';
import { measurements_table_t } from './measurements_table.js';
import { lora_config_t } from './lora_config.js';

export class home_tab_t {
  constructor(dev) {
    this.dev = dev;
  }

  async insert_homepage() {
    const homepage = `
    <fieldset id="home-page-fieldset" class="home-page-fieldset">
        <body>
            <div id="navbar"></div>

            <div class="home-page-div">
                <div class="measurements-table"></div>

                <div class="home-edit-uplink-field" id="home-change-uplink">
                    <h3>Change Uplink Time</h3>
                    <div>
                        <input autocomplete="off" type="number" min="0" id="home-uplink-input"placeholder="Enter a number of minutes"></input>
                        <input id="home-uplink-submit"type="submit">
                    </div>
                </div>

                <div class="lora-config-table">
                    <div class="buttons">
                        <button id="lora-gen-dev-eui">Generate LoRa Dev EUI</button>
                        <button id="lora-gen-app-key">Generate LoRa App Key</button>
                        <button id="lora-send-config">Send</button>
                    </div>
                </div>

                <div class="wifi-config-table"></div>

                <div class="home-load-save-config-buttons">
                    <button  id="home-load-osm-config">Load Configuration</button>
                    <button  id="home-save-osm-config">Save Configuration</button>
                    <button  id="home-disconnect">Disconnect</button>
                </div>

                <div class="home-serial-firmware-config-div">
                    <p id="home-serial-num">Failed to display serial number</p>
                    <p id="home-firmware-version">Failed to display firmware version</p>
                </div>


            </div>
            <script type="module" async src="./modules/gui/navbar.js"></script>
            <script type="module" async src="./modules/gui/measurements_table.js"></script>
            <script type="module" async src="./modules/gui/lora_config.js"></script>
        </body>
    </fieldset>

`;
    const doc = document.getElementById('main-page-body');
    doc.innerHTML = homepage;

    const active_tab = new navbar_t('home');
    active_tab.change_active_tab();

    const meas_table = new measurements_table_t(this.dev);
    await meas_table.create_measurements_table_gui();
    await meas_table.add_uplink_listener();

    const lora = new lora_config_t(this.dev);
    await lora.populate_lora_fields();
    await lora.add_listeners();

    await this.load_serial_number();
    await this.load_fw_ver();
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
    const [, num, hash, , , , , , ,] = this.split_fw;
    this.split_fw_join = `Firmware Version: ${num} - ${hash}`;
    this.fw.textContent = await this.split_fw_join;
  }
}
