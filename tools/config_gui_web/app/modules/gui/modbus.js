export class modbus_t {
  constructor(dev) {
    this.dev = dev;
    this.load_modbus_config = this.load_modbus_config.bind(this);
    this.select_template = this.select_template.bind(this);
    this.modbus_template_table = this.modbus_template_table.bind(this);
    this.current_modbus_config_table = this.current_modbus_config_table.bind(this);
    this.remove_modbus_register = this.remove_modbus_register.bind(this);
    this.remove_modbus_device = this.remove_modbus_device.bind(this);
    this.apply_template = this.apply_template.bind(this);
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
    this.remove_reg_btn = document.getElementById('modbus-current-config-remove-reg');
    this.remove_dev_btn = document.getElementById('modbus-current-config-remove-dev');
    this.mb_bus = await this.dev.modbus_config();
    this.mb_current_config_div = document.getElementById('modbus-current-config');
    const tbl = this.mb_current_config_div.appendChild(document.createElement('table'));
    tbl.id = 'modbus-current-config-table';
    const tbody = tbl.createTBody();
    tbl.createTHead();
    this.checkboxes = [];

    const title = tbl.tHead.insertRow();
    const ti_cell = title.insertCell();
    ti_cell.colSpan = 4;
    ti_cell.textContent = 'Current Modbus Configuration';

    /* Insert headers */
    const row = tbl.tHead.insertRow();
    row.insertCell().textContent = 'Device';
    row.insertCell().textContent = 'Register';
    row.insertCell().textContent = 'Address';
    row.insertCell().textContent = 'Function';

    this.mb_bus.devices.forEach((cell, i) => {
      cell.registers.forEach((j, index) => {
        const r = tbody.insertRow();

        r.insertCell().textContent = cell.name;
        r.insertCell().textContent = j.name;
        r.insertCell().textContent = j.hex;
        r.insertCell().textContent = j.func;
        const chk = document.createElement('input');
        chk.type = 'checkbox';
        const chkcell = r.insertCell();
        chkcell.appendChild(chk);
        this.checkboxes.push(chk);
        chk.addEventListener('click', () => {
          this.remove_reg_btn.disabled = false;
          this.remove_dev_btn.disabled = false;
        });
      });
    });
    this.remove_reg_btn.addEventListener('click', this.remove_modbus_register);
    this.remove_dev_btn.addEventListener('click', this.remove_modbus_device);
  }

  async remove_modbus_register() {
    this.dev_to_remove = '';
    this.list_of_current_devs = [];
    for (let i = 0; i < this.checkboxes.length; i += 1) {
      const chkbox = this.checkboxes[i];
      const is_checked = chkbox.checked;
      if (is_checked === true) {
        const reg = chkbox.parentNode.parentNode.cells[1].textContent;
        this.dev_to_remove = chkbox.parentNode.parentNode.cells[0].textContent;
        await this.dev.remove_modbus_reg(reg);
      }
    }
    this.mb_current_config_div.innerHTML = '';
    await this.current_modbus_config_table();
    this.config_table = document.getElementById('modbus-current-config-table');

    const rows = this.config_table.getElementsByTagName('tr');
    for (let i = 0; i < rows.length; i += 1) {
      const dev = rows[i].cells[0].textContent;
      this.list_of_current_devs.push(dev);
    }
    if (!this.list_of_current_devs.includes(this.dev_to_remove)) {
      await this.dev.remove_modbus_dev(this.dev_to_remove);
    }
    this.remove_reg_btn.disabled = true;
    this.remove_dev_btn.disabled = true;
  }

  async remove_modbus_device() {
    for (let i = 0; i < this.checkboxes.length; i += 1) {
      const chkbox = this.checkboxes[i];
      const is_checked = chkbox.checked;
      if (is_checked === true) {
        const dev = chkbox.parentNode.parentNode.cells[0].textContent;
        await this.dev.remove_modbus_dev(dev);
      }
      this.mb_current_config_div.innerHTML = '';
      await this.current_modbus_config_table();
    }
    this.remove_reg_btn.disabled = true;
    this.remove_dev_btn.disabled = true;
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
      this.modbus_row = tbody.insertRow();
      const cell = this.modbus_row.insertCell();
      cell.textContent = tmp.desc;
      cell.id = 'modbus-device-name-cell';
      cell.addEventListener('click', this.select_template);
      if (count === 0) {
        this.regfield = this.modbus_row.insertCell();
        this.regfield.rowSpan = this.templates.length;
        count += 1;
      }
    });
    const apply_btn = document.getElementById('modbus-template-apply-template-btn');
    apply_btn.addEventListener('click', this.apply_template);
  }

  async select_template(e) {
    this.template = {};
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
    for (let i = 0; i < this.templates.length; i += 1) {
      if (this.templates[i].desc === tmp_name) {
        this.template = this.templates[i];
      }
    }

    this.nested_table = await this.regfield.appendChild(document.createElement('table'));
    this.nested_table.style.width = '100%';
    this.nested_table.style.height = '100%';
    this.nested_table.createTHead();
    const tbody = this.nested_table.createTBody();

    const reg_headers_row = this.nested_table.tHead.insertRow();
    reg_headers_row.insertCell().textContent = 'Name';
    reg_headers_row.insertCell().textContent = 'Address';
    reg_headers_row.insertCell().textContent = 'Type';
    reg_headers_row.insertCell().textContent = 'Unit';

    for (const [key, value] of Object.entries(this.template.registers)) {
      let type = '';
      if (value.function === 3) {
        type = 'Holding Register';
      } else if (value.function === 4) {
        type = 'Input Register';
      }
      let unit_str = '';
      if (value.unit === 'F') {
        unit_str = 'Float';
      } else if (value.unit === 'U32') {
        unit_str = 'U32';
      }
      const nested_row = tbody.insertRow();
      nested_row.insertCell().textContent = key;
      nested_row.insertCell().textContent = value.hex;
      nested_row.insertCell().textContent = type;
      nested_row.insertCell().textContent = unit_str;
    }
  }

  async apply_template() {
    await this.dev.mb_dev_add(
      this.template.unit_id,
      this.template.byteorder,
      this.template.wordorder,
      this.template.name,
    );

    for (const [key, value] of Object.entries(this.template.registers)) {
      await this.dev.mb_reg_add(this.template.unit_id, value.hex, value.function, value.unit, key);
    }

    this.mb_current_config_div.innerHTML = '';
    await this.current_modbus_config_table();
  }
}
