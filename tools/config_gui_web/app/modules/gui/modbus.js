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
    this.add_new_template = this.add_new_template.bind(this);
    this.export_template = this.export_template.bind(this);
    this.import_template_dialog = this.import_template_dialog.bind(this);
    this.load_json_templates = this.load_json_templates.bind(this);
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
    this.checkboxes = [];
    this.remove_reg_btn = document.getElementById('modbus-current-config-remove-reg');
    this.remove_dev_btn = document.getElementById('modbus-current-config-remove-dev');
    this.mb_bus = await this.dev.modbus_config();
    this.mb_current_config_div = document.getElementById('modbus-current-config');
    const tbl = this.mb_current_config_div.appendChild(document.createElement('table'));
    tbl.id = 'modbus-current-config-table';
    const tbody = tbl.createTBody();
    tbl.createTHead();

    const title = tbl.tHead.insertRow();
    const ti_cell = title.insertCell();
    ti_cell.colSpan = 4;
    ti_cell.textContent = 'Latest OSM Modbus Configuration';

    /* Insert headers */
    const row = tbl.tHead.insertRow();
    row.insertCell().textContent = 'Device';
    row.insertCell().textContent = 'Register';
    row.insertCell().textContent = 'Address';
    row.insertCell().textContent = 'Type';

    this.mb_bus.devices.forEach((cell, i) => {
      cell.registers.forEach((j, index) => {
        const r = tbody.insertRow();

        r.insertCell().textContent = cell.name;
        r.insertCell().textContent = j.name;
        r.insertCell().textContent = j.hex;
        let type = '';
        if (j.func === 3) {
          type = 'Holding Register';
        } else if (j.func === 4) {
          type = 'Input Register';
        } else {
          type = j.func;
        }
        r.insertCell().textContent = type;
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
    }
    this.mb_current_config_div.innerHTML = '';
    await this.current_modbus_config_table();
    this.remove_reg_btn.disabled = true;
    this.remove_dev_btn.disabled = true;
  }

  async load_json_templates() {
    this.objs = [];
    this.pub_templates = await fetch('modules/gui/modbus_templates/published_templates.json');
    this.template_json = await this.pub_templates.json();
    for (let i = 0; i < this.template_json.templates.length; i += 1) {
      const tmp = this.template_json.templates[i];
      const obj = await fetch(`modules/gui/modbus_templates/${tmp}`);
      this.objs.push(await obj.json());
    }
    return this.objs;
  }

  async modbus_template_table() {
    this.templates = await this.load_json_templates();
    this.div = document.getElementById('modbus-template-config');
    const tbl = this.div.appendChild(document.createElement('table'));
    tbl.id = 'modbus-template-config-table';
    this.template_tbody = tbl.createTBody();
    tbl.createTHead();

    const title = tbl.tHead.insertRow();
    const title_cell = title.insertCell();
    title_cell.colSpan = 2;
    title_cell.textContent = 'Modbus Device Templates';

    /* Insert headers */
    const row = tbl.tHead.insertRow();
    row.insertCell().textContent = 'Device';
    row.insertCell().textContent = 'Registers';

    let count = 0;
    this.templates.forEach((tmp) => {
      this.modbus_row = this.template_tbody.insertRow();
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
    this.add_new_template_btn = document.getElementById('modbus-template-add-template-btn');
    this.add_new_template_btn.addEventListener('click', this.add_new_template);
    this.import_template_btn = document.getElementById('modbus-template-import-btn');
    this.import_template_btn.addEventListener('click', this.import_template_dialog);
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
    reg_headers_row.insertCell().textContent = 'Datatype';

    for (const [key, value] of Object.entries(this.template.registers)) {
      let type = '';
      if (value.function === 3) {
        type = 'Holding Register';
      } else if (value.function === 4) {
        type = 'Input Register';
      } else {
        type = value.function;
      }
      let datatype_str = '';
      if (value.unit === 'F') {
        datatype_str = 'Float';
      } else {
        datatype_str = value.datatype;
      }
      const nested_row = tbody.insertRow();
      nested_row.insertCell().textContent = key;
      nested_row.insertCell().textContent = value.hex;
      nested_row.insertCell().textContent = type;
      nested_row.insertCell().textContent = datatype_str;
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
      await this.dev.mb_reg_add(
        this.template.unit_id,
        value.hex,
        value.function,
        value.datatype,
        key,
      );
    }

    this.mb_current_config_div.innerHTML = '';
    await this.current_modbus_config_table();
  }

  async add_new_template() {
    this.add_new_div = document.getElementById('modbus-add-new-template-div');
    this.add_new_template_table = this.add_new_div.appendChild(document.createElement('table'));
    this.add_new_template_table.id = 'modbus-add-new-template-table';
    const tbody = this.add_new_template_table.createTBody();
    this.add_new_template_table.createTHead();

    const title = this.add_new_template_table.tHead.insertRow();
    const title_cell = title.insertCell();
    title_cell.textContent = 'Add New Modbus Template';
    title_cell.colSpan = 2;

    let row = tbody.insertRow();

    this.dev_name_cell = row.insertCell();
    this.dev_name_input = document.createElement('input');
    this.dev_name_cell.appendChild(this.dev_name_input);
    this.dev_name_input.placeholder = 'Enter Device Name (Max 4 characters)...';

    this.dev_desc_cell = row.insertCell();
    this.dev_desc_input = document.createElement('input');
    this.dev_desc_cell.appendChild(this.dev_desc_input);
    this.dev_desc_input.placeholder = 'Enter Device Description';

    row = tbody.insertRow();

    this.dev_unit_cell = row.insertCell();
    this.dev_unit_input = document.createElement('input');
    this.dev_unit_input.type = 'number';
    this.dev_unit_cell.appendChild(this.dev_unit_input);
    this.dev_unit_input.placeholder = 'Enter Unit ID';

    this.dev_bits_cell = row.insertCell();
    this.dev_bits_input = document.createElement('input');
    this.dev_bits_input.type = 'number';
    this.dev_bits_input.value = 8;
    this.dev_bits_cell.appendChild(this.dev_bits_input);
    this.dev_bits_input.placeholder = 'Data Bits';

    row = tbody.insertRow();

    this.dev_byteorder_cell = row.insertCell();
    this.dev_byteorder_cell.textContent = 'Byte Order: ';
    this.dev_byteorder_input = document.createElement('select');
    const msb = document.createElement('option');
    msb.text = 'MSB';
    this.dev_byteorder_input.add(msb);
    const lsb = document.createElement('option');
    lsb.text = 'LSB';
    this.dev_byteorder_input.add(lsb);
    this.dev_byteorder_cell.appendChild(this.dev_byteorder_input);

    this.dev_wordorder_cell = row.insertCell();
    this.dev_wordorder_cell.textContent = 'Word Order: ';
    this.dev_wordorder_input = document.createElement('select');
    const msw = document.createElement('option');
    msw.text = 'MSW';
    this.dev_wordorder_input.add(msw);
    const lsw = document.createElement('option');
    lsw.text = 'LSW';
    this.dev_wordorder_input.add(lsw);
    this.dev_wordorder_cell.appendChild(this.dev_wordorder_input);

    row = tbody.insertRow();

    this.dev_parity_cell = row.insertCell();
    this.dev_parity_cell.textContent = 'Parity: ';
    this.dev_parity_input = document.createElement('select');

    const parity_none = document.createElement('option');
    parity_none.text = 'None';
    this.dev_parity_input.add(parity_none);

    const parity_even = document.createElement('option');
    parity_even.text = 'Even';
    this.dev_parity_input.add(parity_even);

    const parity_odd = document.createElement('option');
    parity_odd.text = 'Odd';
    this.dev_parity_input.add(parity_odd);
    this.dev_parity_cell.appendChild(this.dev_parity_input);

    this.dev_stopbits_cell = row.insertCell();
    this.dev_stopbits_input = document.createElement('input');
    this.dev_stopbits_input.type = 'number';
    this.dev_stopbits_input.value = 1;
    this.dev_stopbits_cell.appendChild(this.dev_stopbits_input);
    this.dev_stopbits_input.placeholder = 'Enter Stop Bits';

    row = tbody.insertRow();
    this.add_new_regs_cell = row.insertCell();
    this.add_new_regs_cell.colSpan = 4;

    this.add_new_registers_table = this.add_new_regs_cell.appendChild(document.createElement('table'));
    this.add_new_registers_table.id = 'modbus-add-new-registers-table';
    this.add_new_registers_table.style.width = '100%';
    this.add_new_registers_table.style.height = '100%';
    this.add_new_registers_table.createTHead();

    const reg_title = this.add_new_registers_table.tHead.insertRow();
    const reg_title_cell = reg_title.insertCell();
    reg_title_cell.colSpan = 4;
    reg_title_cell.textContent = 'Registers';

    this.reg_tbody = this.add_new_registers_table.createTBody();

    row = this.reg_tbody.insertRow();
    this.add_new_reg_btn = document.createElement('button');
    const btn_cell = row.insertCell();
    btn_cell.appendChild(this.add_new_reg_btn);
    btn_cell.style.border = '0';
    btn_cell.colSpan = 4;
    this.add_new_reg_btn.textContent = 'Add Reg';
    this.add_new_reg_btn.style.float = 'right';
    this.add_new_reg_btn.style.fontSize = 'x-small';
    this.add_new_reg_btn.addEventListener('click', () => {
      row = this.reg_tbody.insertRow();
      let cell = row.insertCell();
      let input = document.createElement('input');
      input.placeholder = 'Name';
      cell.appendChild(input);

      cell = row.insertCell();
      input = document.createElement('input');
      input.placeholder = 'Address';
      cell.appendChild(input);

      cell = row.insertCell();
      input = document.createElement('input');
      input.placeholder = 'Function Code';
      cell.appendChild(input);

      cell = row.insertCell();
      input = document.createElement('input');
      input.placeholder = 'Datatype';
      cell.appendChild(input);
    });

    this.export_btn = this.add_new_template_table.appendChild(document.createElement('button'));
    this.export_btn.textContent = 'Export';
    this.export_btn.addEventListener('click', this.export_template);
    this.add_new_template_btn.disabled = true;
  }

  async export_template(e) {
    this.json = {};
    this.regs = {};

    if (this.dev_name_input.value.length > 4) {
      this.dev_name_input.value = this.dev_name_input.value.slice(0, 4);
    }
    const name = this.dev_name_input.value;
    const desc = this.dev_desc_input.value;
    const unit_id = this.dev_unit_input.value;
    const bits = this.dev_bits_input.value;
    const byteorder = this.dev_byteorder_input.value;
    const wordorder = this.dev_wordorder_input.value;
    const parity = this.dev_parity_input.value;
    const stopbits = this.dev_stopbits_input.value;

    this.json.name = name;
    this.json.desc = desc;
    this.json.unit_id = unit_id;
    this.json.bits = bits;
    this.json.byteorder = byteorder;
    this.json.wordorder = wordorder;
    this.json.parity = parity;
    this.json.stopbits = stopbits;

    const rows = this.reg_tbody.getElementsByTagName('tr');
    for (let i = 1; i < rows.length; i += 1) {
      const regs = rows[i].getElementsByTagName('input');
      const regname = regs[0].value;
      const addr = regs[1].value;
      const func = Number(regs[2].value);
      const datatypev = regs[3].value;
      this.regs[regname] = { hex: addr, function: func, datatype: datatypev };
    }
    this.json.registers = await this.regs;

    const json_data = JSON.stringify(this.json);
    await this.download_template(json_data, `${name}.json`, 'application/json');
    await this.import_template(json_data);
    this.add_new_div.innerHTML = '';
    this.add_new_template_btn.disabled = false;
  }

  async download_template(content, file_name, content_type) {
    this.a = document.createElement('a');
    const file = new Blob([content], { type: content_type });
    this.a.href = URL.createObjectURL(file);
    this.a.download = file_name;
    this.a.click();
  }

  async import_template_dialog() {
    this.import_template_dialog = document.createElement('input');
    this.import_template_dialog.type = 'file';
    this.import_template_dialog.accept = '.json';
    this.import_template_dialog.onchange = (_) => {
      const reader = new FileReader();
      const file = Array.from(this.import_template_dialog.files)[0];
      if (file.name.includes('.json')) {
        reader.readAsText(file, 'application/json');
        reader.onload = (e) => { this.import_template(e.target.result); };
        reader.onerror = (error) => {
          console.log(`Error: ${error}`);
        };
      } else {
        console.log(`Unsupported filetype '${file.type}' selected.`);
      }
    };
    this.import_template_dialog.click();
  }

  async import_template(content) {
    this.template_json = await JSON.parse(content);
    this.templates.push(this.template_json);

    this.import_row = this.template_tbody.insertRow();
    const cell = this.import_row.insertCell();
    cell.textContent = this.template_json.desc;
    this.regfield.rowSpan = this.templates.length;
    cell.addEventListener('click', this.select_template);
  }
}
