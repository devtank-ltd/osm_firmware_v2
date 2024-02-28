import { generate_random } from '../backend/binding.js';
import { disable_interaction, limit_characters } from './disable.js';

export class lora_config_t {
  constructor(comms) {
    this.comms = comms;
    this.write_config = this.write_config.bind(this);
  }

  async get_region_html() {
    this.resp = await fetch('modules/gui/html/region_dropdown.html');
    this.text = await this.resp.text();
    return this.text;
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

    const dev_eui = await this.comms.lora_deveui;
    const app_key = await this.comms.lora_appkey;
    const region = await this.comms.lora_region;
    let conn = await this.comms.comms_conn;
    const region_dropdwn = await this.get_region_html();

    const lora_res = document.querySelector('div.lora-config-table');
    lora_res.style.display = 'block';

    const lora_btns = document.getElementById('lora-btns');
    lora_btns.style.display = 'flex';
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
        td.contentEditable = true;
        td.spellcheck = false;
        td.oninput = (e) => { limit_characters(e, 16); };
        td.id = 'lora-dev-eui-value';
      } else if (i === 'Application Key') {
        const td = r.insertCell();
        td.textContent = app_key;
        td.contentEditable = true;
        td.spellcheck = false;
        td.oninput = (e) => { limit_characters(e, 32); };
        td.id = 'lora-app-key-value';
      } else if (i === 'Region') {
        const td = r.insertCell();
        td.innerHTML = region_dropdwn;
        const sel = document.getElementById('lora-config-region-dropdown');
        for (let v = 0; v < sel.length; v += 1) {
          if (region.includes(sel[v].innerHTML)) {
            sel.selectedIndex = v;
          }
        }
      } else if (i === 'Status') {
        const td = r.insertCell();
        conn = (conn === '1 | Connected') ? 'Connected' : 'Disconnected';
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
    this.deveui_field = document.getElementById('lora-dev-eui-value');
    const deveui = await generate_random(16);
    this.deveui_field.textContent = deveui;
  }

  async write_config() {
    await disable_interaction(true);
    const loramsg = document.getElementById('lora-msg-div');
    loramsg.textContent = '';
    const deveui = document.getElementById('lora-dev-eui-value').innerHTML;
    const appkey = document.getElementById('lora-app-key-value').innerHTML;
    const reg = document.getElementById('lora-config-region-dropdown').value.split(' ')[0];

    this.comms.lora_deveui = deveui;
    this.comms.lora_appkey = appkey;
    this.comms.lora_region = reg;
    loramsg.textContent = 'Configuration sent.'
    await disable_interaction(false);
  }
}
