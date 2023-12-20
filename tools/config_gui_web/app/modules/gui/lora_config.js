import { generate_random } from '../backend/binding.js';

export class lora_config_t {
  constructor(dev) {
    this.dev = dev;
    this.write_config = this.write_config.bind(this);
    this.region_dropdown = `
            <select class="lora-config-region-dropdown" id="lora-config-region-dropdown">
                <option>EU433 (0)</option>
                <option>CN470 (1)</option>
                <option>RU864 (2)</option>
                <option>IN865 (3)</option>
                <option>EU868 (4)</option>
                <option>US915 (5)</option>
                <option>AU915 (6)</option>
                <option>KR920 (7)</option>
                <option>AS923-1 (8)</option>
                <option>AS923-2 (9)</option>
                <option>AS923-3 (10)</option>
                <option>AS923-4 (11)</option>
            </select>
        `;
  }

  async add_listeners() {
    const deveui = document.getElementById('lora-gen-dev-eui');
    deveui.onclick = this.random_deveui;

    const appbtn = document.getElementById('lora-gen-app-key');
    appbtn.onclick = this.random_appkey;

    const sendbtn = document.getElementById('lora-send-config');
    sendbtn.onclick = this.write_config;
  }

  async populate_lora_fields() {
    const title = 'LoRaWAN Configuration';
    const lora_headers = ['Device EUI', 'Application Key', 'Region', 'Status'];

    const dev_eui = await this.dev.lora_deveui;
    const app_key = await this.dev.lora_appkey;
    const region = await this.dev.lora_region;
    let conn = await this.dev.comms_conn;

    const lora_res = document.querySelector('div.lora-config-table');
    const lora_tbl = lora_res.appendChild(document.createElement('table'));
    const lora_tBody = lora_tbl.createTBody();
    lora_tbl.createTHead();

    const lora_row = lora_tbl.tHead.insertRow();
    const cell = lora_row.insertCell();
    cell.colSpan = 2;
    cell.textContent = title;

    lora_headers.forEach((i) => {
      const r = lora_tBody.insertRow();
      r.insertCell().textContent = i;
      if (i === 'Device EUI') {
        const td = r.insertCell();
        td.textContent = dev_eui;
        td.id = 'lora-dev-eui-value';
      } else if (i === 'Application Key') {
        const td = r.insertCell();
        td.textContent = app_key;
        td.id = 'lora-app-key-value';
      } else if (i === 'Region') {
        const td = r.insertCell();
        td.innerHTML = this.region_dropdown;
        const sel = document.getElementById('lora-config-region-dropdown');
        for (let v = 0; v < sel.length; v += 1) {
          if (region === sel[v].innerHTML) {
            sel.selectedIndex = v;
          }
        }
      } else if (i === 'Status') {
        const td = r.insertCell();
        if (conn === '0 | Disconnected') { conn = 'Disconnected'; } else if (conn === '1 | Connected') { conn = 'Connected'; }
        td.textContent = conn;
        td.id = 'lora-status-value';
      }
    });
  }

  async random_appkey() {
    this.appfield = document.getElementById('lora-app-key-value');
    const appkey = await generate_random(32);
    this.appfield.textContent = appkey;
  }

  async random_deveui() {
    this.devfield = document.getElementById('lora-dev-eui-value');
    const deveui = await generate_random(16);
    this.devfield.textContent = deveui;
  }

  async write_config() {
    const deveui = document.getElementById('lora-dev-eui-value').innerHTML;
    const appkey = document.getElementById('lora-app-key-value').innerHTML;
    const reg = document.getElementById('lora-config-region-dropdown').value.split(' ')[0];

    this.dev.lora_deveui = deveui;
    this.dev.lora_appkey = appkey;
    this.dev.lora_region = reg;
  }
}
