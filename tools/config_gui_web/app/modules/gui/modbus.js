export class modbus_t {
  constructor(dev) {
    this.dev = dev;
    this.load_modbus_config = this.load_modbus_config.bind(this);
    this.select_template = this.select_template.bind(this);
    this.modbus_template_table = this.modbus_template_table.bind(this);
  }

  async open_modbus() {
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/modbus.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    this.load_modbus_config();
  }

  async load_modbus_config() {
    this.mb_bus = await this.dev.modbus_config();
    this.current_modbus_config_table();
    this.modbus_template_table();
  }

  async current_modbus_config_table() {
    this.mb_bus = await this.dev.modbus_config();
    this.div = document.getElementById('modbus-current-config');
    const tbl = this.div.appendChild(document.createElement('table'));
    tbl.id = 'modbus-current-config-table';
    const tbody = tbl.createTBody();
    tbl.createTHead();

    const title = tbl.tHead.insertRow();
    const ti_cell = title.insertCell();
    ti_cell.colSpan = 4;
    ti_cell.textContent = 'Current Modbus Configuration';

    /* Insert headers */
    const row = tbl.tHead.insertRow();
    row.insertCell().textContent = 'Device';
    row.insertCell().textContent = 'Address';
    row.insertCell().textContent = 'Function';
    row.insertCell().textContent = 'Name';

    this.mb_bus.devices.forEach((cell, i) => {
      cell.registers.forEach((j, index) => {
        const r = tbody.insertRow();

        r.insertCell().textContent = cell.name;
        r.insertCell().textContent = j.hex;
        r.insertCell().textContent = j.func;
        r.insertCell().textContent = j.name;
      });
    });
  }

  async load_json_templates() {
    this.obj = [];
    this.rayleigh = await fetch('modules/gui/modbus_templates/rayleigh_rif.json');
    this.obj.push(await this.rayleigh.json());
    this.e53 = await fetch('modules/gui/modbus_templates/countis_e53.json');
    this.obj.push(await this.e53.json());
    return this.obj;
  }

  async modbus_template_table() {
    this.templates = await this.load_json_templates();
    this.div = document.getElementById('modbus-template-config');
    const tbl = this.div.appendChild(document.createElement('table'));
    tbl.id = 'modbus-template-config-table';
    const tbody = tbl.createTBody();
    tbl.createTHead();

    const title = tbl.tHead.insertRow();
    const ti_cell = title.insertCell();
    ti_cell.colSpan = 2;
    ti_cell.textContent = 'Modbus Device Templates';

    /* Insert headers */
    const row = tbl.tHead.insertRow();
    row.insertCell().textContent = 'Device';
    row.insertCell().textContent = 'Registers';

    let count = 0;
    this.templates.forEach((tmp) => {
      const r = tbody.insertRow();

      const cell = r.insertCell();
      cell.textContent = tmp.name;
      cell.id = 'modbus-device-name-cell';
      cell.addEventListener('click', this.select_template);
      if (count === 0) {
        this.regfield = r.insertCell();
        this.regfield.rowSpan = this.templates.length;
        this.regfield.classList.add('modbus-template-config-table-reg-field');
        count += 1;
      }
    });
  }

  async select_template(e) {
    this.regfield.textContent = '';
    this.tbl = e.target.parentNode.parentNode.parentNode;
    const row_id = e.target.parentNode.rowIndex;
    const rows_not_selected = this.tbl.getElementsByTagName('tr');
    for (let rown = 0; rown < rows_not_selected.length; rown += 1) {
      rows_not_selected[rown].cells[0].style.backgroundColor = '';
      rows_not_selected[rown].cells[0].classList.remove('selected-cell');
    }

    const row_selected = this.tbl.getElementsByTagName('tr')[row_id].cells[0];
    row_selected.classList.add('selected-cell');

    const tmp_name = row_selected.textContent;
    this.template = {};
    for (let i = 0; i < this.templates.length; i += 1) {
      if (this.templates[i].name === tmp_name) {
        this.template = this.templates[i];
      }
    }

    for (const [key, value] of Object.entries(this.template.registers)) {
      this.regfield.textContent += `Name: ${key}, Address: ${value.hex},
      Function: ${value.function}, Unit: ${value.unit}\n\n`;
    }
  }
}
