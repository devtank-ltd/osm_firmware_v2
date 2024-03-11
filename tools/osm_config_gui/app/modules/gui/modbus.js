import { disable_interaction, limit_characters } from './disable.js';

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
        this.reload_template_table = this.reload_template_table.bind(this);
        this.convert_hex_addr = this.convert_hex_addr.bind(this);
    }

    async open_modbus() {
        await disable_interaction(true);
        await this.load_modbus_config();
        await disable_interaction(false);
    }

    async load_modbus_config() {
        await disable_interaction(true);
        this.mb_bus = await this.dev.modbus_config();
        await this.current_modbus_config_table();
        await this.modbus_template_table();
        await this.add_new_template();
        await disable_interaction(false);
    }

    async current_modbus_config_table() {
        await disable_interaction(true);
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
            cell.registers.forEach(async (j, _index) => {
                const r = tbody.insertRow();

                r.insertCell().textContent = cell.name;
                r.insertCell().textContent = j.name;
                r.insertCell().textContent = j.hex;
                const type = await this.function_code_check(j.func);
                r.insertCell().textContent = type;
                const chk = document.createElement('input');
                chk.type = 'checkbox';
                const chkcell = r.insertCell();
                chkcell.appendChild(chk);
                this.checkboxes.push(chk);
            });
        });
        this.remove_reg_btn.addEventListener('click', this.remove_modbus_register);
        this.remove_dev_btn.addEventListener('click', this.remove_modbus_device);
        await disable_interaction(false);
    }

    async function_code_check(func) {
        let type;
        switch (func) {
            case 1:
                type = 'Coil';
                break;
            case 2:
                type = 'Discrete Input';
                break;
            case 3:
                type = 'Holding Register';
                break;
            case 4:
                type = 'Input Register';
                break;
            default:
                type = 'Unknown';
                break;
        }
        return type;
    }

    async remove_modbus_register() {
        await disable_interaction(true);
        this.dev_to_remove = '';
        let found = false;
        this.list_of_current_devs = [];
        for (let i = 0; i < this.checkboxes.length; i += 1) {
            const chkbox = this.checkboxes[i];
            const is_checked = chkbox.checked;
            if (is_checked === true) {
                found = true;
                const reg = chkbox.parentNode.parentNode.cells[1].textContent;
                this.dev_to_remove = chkbox.parentNode.parentNode.cells[0].textContent;
                await this.dev.remove_modbus_reg(reg);
            }
        }
        if (!found) {
            this.modbus_modal('Select a register.');
            await disable_interaction(false);
            return;
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
        await disable_interaction(false);
    }

    async remove_modbus_device() {
        await disable_interaction(true);
        let found = false;
        for (let i = 0; i < this.checkboxes.length; i += 1) {
            const chkbox = this.checkboxes[i];
            const is_checked = chkbox.checked;
            if (is_checked === true) {
                found = true;
                const dev = chkbox.parentNode.parentNode.cells[0].textContent;
                await this.dev.remove_modbus_dev(dev);
            }
        }
        if (!found) {
            this.modbus_modal('Select a register.');
            await disable_interaction(false);
            return;
        }
        this.mb_current_config_div.innerHTML = '';
        await this.current_modbus_config_table();
        this.remove_reg_btn.disabled = true;
        this.remove_dev_btn.disabled = true;
        await disable_interaction(false);
    }

    async load_json_templates() {
        await disable_interaction(true);
        this.objs = [];
        this.pub_templates = await fetch('modules/gui/modbus_templates/published_templates.json');
        this.template_json = await this.pub_templates.json();
        for (let i = 0; i < this.template_json.templates.length; i += 1) {
            const tmp = this.template_json.templates[i];
            const obj = await fetch(`modules/gui/modbus_templates/${tmp}`);
            this.objs.push(await obj.json());
        }
        await disable_interaction(false);
        return this.objs;
    }

    async modbus_template_table() {
        await disable_interaction(true);
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
            cell.style.cursor = 'pointer';
            cell.addEventListener('click', this.select_template);
            if (count === 0) {
                this.regfield = this.modbus_row.insertCell();
                this.regfield.rowSpan = this.templates.length;
                count += 1;
            }
        });
        const apply_btn = document.getElementById('modbus-template-apply-template-btn');
        apply_btn.addEventListener('click', this.apply_template);
        this.import_template_btn = document.getElementById('modbus-template-import-btn');
        this.import_template_btn.addEventListener('click', this.import_template_dialog);
        await disable_interaction(false);
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
            const type = await this.function_code_check(value.function);
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

    async apply_template(e) {
        if (this.template) {
            await disable_interaction(true);
            await this.dev.mb_dev_add(
                this.template.unit_id,
                this.template.byteorder,
                this.template.wordorder,
                this.template.name,
            );

            for (const [key, value] of Object.entries(this.template.registers)) {
                let datat;
                if (value.datatype === 'Float') {
                    datat = 'F'
                }
                else {
                    datat = value.datatype;
                }
                await this.dev.mb_reg_add(
                    this.template.unit_id,
                    value.hex,
                    value.function,
                    datat,
                    key,
                );
            }

            this.mb_current_config_div.innerHTML = '';
            await this.current_modbus_config_table();
            await disable_interaction(false);
        }
        else {
            this.modbus_modal('Select a template.');
        }
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
        this.dev_name_cell.oninput = (e) => { limit_characters(e, 4); };

        this.dev_desc_cell = row.insertCell();
        this.dev_desc_input = document.createElement('input');
        this.dev_desc_input.oninput = (e) => { limit_characters(e, 32); };
        this.dev_desc_cell.appendChild(this.dev_desc_input);
        this.dev_desc_input.placeholder = 'Enter Device Description';

        row = tbody.insertRow();

        this.dev_unit_cell = row.insertCell();
        this.dev_unit_input = document.createElement('input');
        this.dev_unit_input.type = 'number';
        this.dev_unit_input.oninput = (e) => { limit_characters(e, 3); };
        this.dev_unit_cell.appendChild(this.dev_unit_input);
        this.dev_unit_input.placeholder = 'Enter Unit ID';

        this.dev_bits_cell = row.insertCell();
        this.dev_bits_input = document.createElement('input');
        this.dev_bits_input.type = 'number';
        this.dev_bits_input.value = 8;
        this.dev_bits_input.oninput = (e) => { limit_characters(e, 3); };
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
        this.dev_stopbits_input.oninput = (e) => { limit_characters(e, 1); };
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
            cell.oninput = (e) => { limit_characters(e, 4); };
            cell.appendChild(input);

            cell = row.insertCell();
            input = document.createElement('input');
            input.placeholder = 'Address';
            input.title = 'A decimal address will be converted to its corresponding hex address and will automatically select the matching function code.'
            input.oninput = (e) => { limit_characters(e, 6); };
            input.addEventListener('focusout', this.convert_hex_addr)
            cell.appendChild(input);

            cell = row.insertCell();
            input = document.createElement('select');
            input.title = "Function Code"
            const func = document.createElement('option');
            func.text = 'Coil (1)';
            func.value = 1;
            const func2 = document.createElement('option');
            func2.text = 'Discrete Input (2)';
            func2.value = 2;
            const func3 = document.createElement('option');
            func3.text = 'Input Register (3)';
            func3.value = 3;
            const func4 = document.createElement('option');
            func4.text = 'Holding Register (4)';
            func4.value = 4;
            input.add(func);
            input.add(func2);
            input.add(func3);
            input.add(func4);
            cell.appendChild(input);

            cell = row.insertCell();
            input = document.createElement('select');
            input.title = "Data Type"
            const datatf = document.createElement('option');
            datatf.text = 'Float';
            const datati32 = document.createElement('option');
            datati32.text = 'I32';
            const datati16 = document.createElement('option');
            datati16.text = 'I16';
            const datatu32 = document.createElement('option');
            datatu32.text = 'U32';
            const datatu16 = document.createElement('option');
            datatu16.text = 'U16';
            input.add(datatf);
            input.add(datati32);
            input.add(datati16);
            input.add(datatu32);
            input.add(datatu16);
            cell.appendChild(input);
        });
        this.add_new_reg_btn.click();

        this.export_btn = document.getElementById('modbus-add-new-export');
        this.export_btn.addEventListener('click', this.export_template);

        this.restart_btn = document.getElementById('modbus-add-new-restart');
        this.restart_btn.addEventListener('click', this.reload_template_table);
    }

    async reload_template_table() {
        this.add_new_div = document.getElementById('modbus-add-new-template-div');
        this.add_new_div.innerHTML = '';
        await this.add_new_template();
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
            const options = rows[i].getElementsByTagName('select');
            const regname = regs[0].value;
            const addr = regs[1].value;
            const func = Number(options[0].value);
            const datatypev = options[1].value;
            this.regs[regname] = { hex: addr, function: func, datatype: datatypev };
        }
        this.json.registers = await this.regs;
        const ver = await this.verify_template(this.json);
        if (ver) {
            const json_data = JSON.stringify(this.json);
            await this.download_template(json_data, `${name}.json`, 'application/json');
            await this.import_template(json_data);
        }
        else {
            this.modbus_modal('Incomplete template.')
            return;
        }
        await this.reload_template_table();
    }

    async verify_template(data) {
        this.bool = false;
        for (const [key, value] of Object.entries(data)) {
            if (!value) {
                return this.bool;
            }
            if (key === 'registers') {
                for (const [regkey, regvalue] of Object.entries(value)) {
                    if (!regkey || !regvalue.hex || !regvalue.datatype || !regvalue.function) {
                        return this.bool;
                    }
                }
            }
        }
        this.bool = true;
        return this.bool;
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
        const ver = await this.verify_template(this.template_json);
        if (ver) {
            this.templates.push(this.template_json);

            this.import_row = this.template_tbody.insertRow();
            const cell = this.import_row.insertCell();
            cell.textContent = this.template_json.desc;
            this.regfield.rowSpan = this.templates.length;
            cell.addEventListener('click', this.select_template);
        }
        else {
            this.modbus_modal('Invalid JSON file.')
        }
    }

    async modbus_modal(msg) {
        this.dialog = document.getElementById('modbus-dialog');
        this.confirm = document.getElementById('modbus-dialog-confirm');
        this.msg = document.getElementById('modbus-dialog-msg');
        this.msg.textContent = msg;
        this.dialog.showModal();
        const controller = new AbortController();

        this.confirm.addEventListener('click', async () => {
            this.dialog.close();
            controller.abort();
        });
    }

    async convert_hex_addr(e) {
        this.input = e.target;
        this.input_value = e.target.value;
        this.function_dropdwn = e.target.parentNode.parentNode.cells[2].firstChild;

        this.function_code = await this.extract_function_code(this.input_value);
        if (this.function_code) {
            this.function_dropdwn.value = this.function_code;
            this.converted = await this.convert_decimal_to_hex(this.input_value);
            if (this.converted) {
                this.input.value = this.converted;
            }
        }
    }

    async decimal_to_hex(decimal) {
        const dec = parseInt(decimal);
        let hexstr = '0x'
        return hexstr.concat(dec.toString(16));
    }

    async extract_function_code(decimal) {
        let dec_str = decimal.toString();
        let function_code;

        switch (dec_str[0]) {
            case '1':
                function_code = 1;
                break;
            case '2':
                function_code = 2;
                break;
            case '3':
                function_code = 4;
                break;
            case '4':
                function_code = 3;
                break;
            default:
                function_code = false;
                break;
        }
        return function_code;
    }

    async convert_decimal_to_hex(dec_str) {
        const hex_str = await this.get_hex(dec_str.substring(1))
        if (hex_str) {
            let finalstr = await this.decimal_to_hex(hex_str);
            return finalstr;
        }
        return false;
    }

    async get_hex(stringc) {
        let extracted = '';
        for (let i = 0; i < stringc.length; i += 1) {
            if (stringc[i] !== '0') {
                    if (extracted) {
                        extracted += stringc[i];
                    }
                    else {
                        extracted = stringc[i];
                    }
            }
        }
        return extracted;
    }

}
