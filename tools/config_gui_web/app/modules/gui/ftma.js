import { disable_interaction, limit_characters } from './disable.js';

export class ftma_t {
    constructor(dev) {
        this.dev = dev;
    }

    async open_ftma_tab() {
        await disable_interaction(true);
        this.doc = document.getElementById('main-page-body');
        this.response = await fetch('modules/gui/html/ftma.html');
        this.text = await this.response.text();
        this.doc.innerHTML = this.text;
        this.create_ftma_table();
        await disable_interaction(false);
    }

    async get_ftma_specs() {
        this.specs = await this.dev.get_ftma_types();
        return this.specs;
    }

    async create_ftma_table() {
        this.ftma_div = document.getElementById('ftma-div');
        this.ftma_div.innerHTML = '';
        this.add_ftma_Table = this.ftma_div.appendChild(document.createElement('table'));
        this.add_ftma_Table.id = 'ftma-table';
        this.add_ftma_Table.classList = 'ftma-table';
        const tbody = this.add_ftma_Table.createTBody();
        this.add_ftma_Table.createTHead();

        const title = this.add_ftma_Table.tHead.insertRow();
        const title_cell = title.insertCell();
        title_cell.textContent = '4-20mA Coefficients (y = A + Bx + Cx² + Dx³)';
        title_cell.colSpan = 6;

        const headers = this.add_ftma_Table.tHead.insertRow();
        headers.insertCell().textContent = 'Measurement';
        headers.insertCell().textContent = 'A';
        headers.insertCell().textContent = 'B';
        headers.insertCell().textContent = 'C';
        headers.insertCell().textContent = 'D';
        headers.insertCell().textContent = 'Send';

        this.measurements = await this.get_ftma_specs();
        for (let i = 0; i < this.measurements.length; i += 1) {
            const meas = this.measurements[i];
            const row = tbody.insertRow();
            this.c = await this.dev.get_ftma_coeffs(meas);
            this.coeffs = this.c.slice(1, -1);
            const meas_cell = row.insertCell();
            meas_cell.textContent = meas;
            meas_cell.contentEditable = true;
            meas_cell.oninput = (e) => { limit_characters(e, 4); };
            meas_cell.addEventListener('focusout', (e) => { this.change_ftma_name(meas, e); });
            this.coeffs.forEach((coeff) => {
                const regex = /[A-D]: (\d+\.\d+)/;
                const [, match] = coeff.match(regex);
                if (match) {
                    const ftma_cell = row.insertCell();
                    ftma_cell.contentEditable = true;
                    ftma_cell.textContent = match;
                }
            });
            const chk_cell = row.insertCell();
            const chk = document.createElement('input');
            chk.type = 'checkbox';
            chk_cell.appendChild(chk);
            chk.addEventListener('click', async (e) => {
                await this.change_ftma_coeffs(meas, e);
            });
        }
    }

    async change_ftma_name(meas, e) {
        await disable_interaction(true);
        this.new_name = e.target.textContent;
        if (this.new_name !== meas) {
            disable_interaction(true);
            await this.dev.set_ftma_name(meas, this.new_name);
            await this.create_ftma_table();
        }
        await disable_interaction(false);
    }

    async change_ftma_coeffs(meas, e) {
        await disable_interaction(true);
        const checkbox = e.target;
        this.coeff_a = checkbox.parentNode.parentNode.cells[1].textContent;
        this.coeff_b = checkbox.parentNode.parentNode.cells[2].textContent;
        this.coeff_c = checkbox.parentNode.parentNode.cells[3].textContent;
        this.coeff_d = checkbox.parentNode.parentNode.cells[4].textContent;
        await this.dev.set_ftma_coeffs(meas, this.coeff_a, this.coeff_b, this.coeff_c, this.coeff_d);
        await this.draw_ftma_coeff_graph(meas);
        checkbox.checked = false;
        await disable_interaction(false);
    }

    async draw_ftma_coeff_graph(meas) {
        const dis = document.getElementById('global-fieldset');
        const canvas_id = 'coeff-canvas';
        this.canvas = document.getElementById(canvas_id);
        const ctx = this.canvas.getContext('2d');
        ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

        const X_START = 170;
        const X_END = 440;
        const Y_START = 260;
        const Y_END = 50;
        const X_OFFSET = 50;

        ctx.beginPath();
        ctx.moveTo(X_START, Y_START); // x axis
        ctx.lineTo(X_END, Y_START);
        ctx.moveTo(X_START, Y_START); // y axis
        ctx.lineTo(X_START, Y_END);
        ctx.stroke();

        const EQU_X_OFFSET = 110;
        const EQU_Y_OFFSET = -140;

        const X_AXIS_LAB_X_OFFSET = -70;
        const X_AXIS_LAB_Y_OFFSET = -110;

        const Y_AXIS_LAB_X_OFFSET = 125;
        const Y_AXIS_LAB_Y_OFFSET = 30;

        // title
        ctx.fillText(meas, X_START + EQU_X_OFFSET, X_START + EQU_Y_OFFSET);
        // y axis label
        ctx.save();
        ctx.translate(X_START + X_AXIS_LAB_X_OFFSET, Y_START + X_AXIS_LAB_Y_OFFSET);
        ctx.rotate(-Math.PI / 2);
        ctx.fillText('Output', 0, 0);
        ctx.restore();
        // x axis label
        ctx.fillText('mA', X_START + Y_AXIS_LAB_X_OFFSET, Y_START + Y_AXIS_LAB_Y_OFFSET);

        const MA_MIN = 4;
        const MA_MAX = 20;

        const unit_min_y = await this.calculate_output(meas, MA_MIN);
        const unit_max_y = await this.calculate_output(meas, MA_MAX);

        const pixel_y_range = Y_START - Y_END;
        const unit_y_range = unit_max_y - unit_min_y;

        const unit_to_pixel_y = pixel_y_range / unit_y_range;

        const pixel_x_range = X_END - X_START;
        const ma_x_range = MA_MAX - MA_MIN;

        const unit_to_pixel_x = pixel_x_range / ma_x_range;

        const PNT_SIZE = 3;

        // Draw the X axis labels
        for (let mA = MA_MIN; mA <= MA_MAX; mA += 4) {
            const x = X_START + (mA - MA_MIN) * ((X_END - X_START) / (MA_MAX - MA_MIN));
            ctx.fillText(mA.toString(), x, Y_START + 10);
            ctx.moveTo(x, Y_START);
            ctx.lineTo(x, Y_END);
            ctx.stroke();
        }

        const Y_STEPS = 10;
        const OUTPUT_STEP_SCALE = (Y_START - Y_END) / Y_STEPS;

        // Draw the Y axis labels
        for (let i = 0; i <= Y_STEPS; i += 1) {
            const output = unit_min_y + i * OUTPUT_STEP_SCALE;
            const y = Y_START - i * ((Y_START - Y_END) / Y_STEPS);
            ctx.moveTo(X_START, y);
            ctx.lineTo(X_END, y);
            ctx.stroke();
            ctx.fillText(output, X_START - X_OFFSET, y);
        }

        // Draw the data points
        for (let mA = MA_MIN; mA <= MA_MAX; mA += 1) {
            const output = await this.calculate_output(meas, mA);
            const x = X_START + ((mA - MA_MIN) * unit_to_pixel_x);
            const y = Y_START - ((output - unit_min_y) * unit_to_pixel_y);
            ctx.beginPath();
            ctx.arc(x, y, PNT_SIZE, 0, 2 * Math.PI);
            ctx.fillStyle = 'blue';
            ctx.fill();
            ctx.stroke();
        }
    }

    async calculate_output(meas, milliamps) {
        this.ma = parseFloat(milliamps);
        this.coeff_list = [];
        this.c = await this.dev.get_ftma_coeffs(meas);
        this.coeffs = this.c.slice(1, -1);
        this.coeffs.forEach((coeff) => {
            const regex = /[A-D]: (\d+\.\d+)/;
            const [, match] = coeff.match(regex);
            if (match) {
                const parsed = parseFloat(match);
                this.coeff_list.push(parsed);
            }
        });
        const [A, B, C, D] = this.coeff_list;
        const output = A + B * this.ma + C * this.ma ** 2 + D * this.ma ** 3;
        return output;
    }
}
