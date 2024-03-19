import { disable_interaction } from './disable.js';

export class current_clamp_t {
    constructor(dev) {
        this.dev = dev;
        this.create_cc_table = this.create_cc_table.bind(this);
        this.open_cc = this.open_cc.bind(this);
        this.open_cc_modal = this.open_cc_modal.bind(this);
        this.set_cc_exterior = this.set_cc_exterior.bind(this);
        this.set_cc_interior = this.set_cc_interior.bind(this);
        this.set_cc_midpoint = this.set_cc_midpoint.bind(this);
    }

    async open_cc() {
        await disable_interaction(true);
        await this.create_cc_table();
        await this.add_event_listeners();
        await disable_interaction(false);
    }

    async add_event_listeners() {
        document.getElementById('current-clamp-cal-btn').addEventListener('click', this.open_cc_modal);
    }

    async open_cc_modal() {
        this.dialog = document.getElementById('cc-cal-dialog');
        this.confirm = document.getElementById('cc-modal-confirm');
        this.cancel = document.getElementById('cc-modal-cancel');

        this.dialog.showModal();
        this.confirm.addEventListener('click', async () => {
            const loader = document.getElementById('loader');
            loader.style.display = 'block';
            await this.dev.cc_cal();
            await this.create_cc_table();
            loader.style.display = 'none';
        });
        this.cancel.addEventListener('click', () => {
            this.dialog.close();
        });
    }

    async create_cc_table() {
        await disable_interaction(true);
        this.cc_div = document.getElementById('current-clamp-div');
        this.cc_div.innerHTML = '';
        this.add_cc_table = this.cc_div.appendChild(document.createElement('table'));
        this.add_cc_table.id = 'current-clamp-table';
        this.add_cc_table.classList = 'current-clamp-table';
        const tbody = this.add_cc_table.createTBody();
        this.add_cc_table.createTHead();

        const gain = await this.dev.get_cc_gain();
        const phase_one_mp = await this.dev.get_cc_mp(1);
        const phase_two_mp = await this.dev.get_cc_mp(2);
        const phase_three_mp = await this.dev.get_cc_mp(3);

        const phase_one_type = await this.dev.get_cc_type(1);
        const phase_two_type = await this.dev.get_cc_type(2);
        const phase_three_type = await this.dev.get_cc_type(3);

        const title = this.add_cc_table.tHead.insertRow();
        const title_cell = title.insertCell();
        title_cell.textContent = 'Current Clamp Configuration';
        title_cell.colSpan = 4;

        const headers = this.add_cc_table.tHead.insertRow();
        headers.insertCell().textContent = 'Measurement';
        headers.insertCell().textContent = 'Primary';
        headers.insertCell().textContent = 'Secondary';
        headers.insertCell().textContent = 'Midpoint';

        const top_level = [];
        let measurements = [];

        gain.forEach((i, index) => {
            const meas_regex = /^(CC\d+)/;
            const meas_match = i.match(meas_regex);
            if (meas_match) {
                const meas = meas_match[1];
                if (!measurements.includes(meas)) {
                    measurements = [];
                    measurements.push(meas);
                }
            }

            const ext_regex = /EXT max:\s(\d+(\.\d+)?)/;
            const match = i.match(ext_regex);

            if (match) {
                const extracted = match[1];
                measurements.push(extracted);
            } else {
                const int_regex = /INT max:\s(\d+(\.\d+)?)/;
                const int_match = i.match(int_regex);
                if (int_match) {
                    const int_extracted = int_match[1];
                    measurements.push(int_extracted);

                    if (index <= 1) {
                        measurements.push(phase_one_mp);
                        top_level.push(measurements);
                    } else if (index <= 3) {
                        measurements.push(phase_two_mp);
                        top_level.push(measurements);
                    } else if (index <= 5) {
                        measurements.push(phase_three_mp);
                        top_level.push(measurements);
                    }
                }
            }
        });

        top_level.forEach(async (i, index) => {
            const phase = index + 1;
            let cc_type;
            if (phase === 1) {
                cc_type = phase_one_type;
            }
            else if (phase === 2) {
                cc_type = phase_two_type;
            }
            else if (phase === 3) {
                cc_type = phase_three_type;
            }
            const cc_row = tbody.insertRow();
            const [meas, ext, int, mp] = i;

            const int_p = parseInt(parseFloat(int) * 1000, 10);
            const ext_p = parseInt(ext, 10);
            cc_row.insertCell().textContent = meas;
            const ext_cell = cc_row.insertCell();

            const ext_cell_sel = document.createElement('select');
            const opt1 = document.createElement('option');
            opt1.text = 25;
            const opt2 = document.createElement('option');
            opt2.text = 50;
            const opt3 = document.createElement('option');
            opt3.text = 75;
            const opt4 = document.createElement('option');
            opt4.text = 100;
            ext_cell_sel.add(opt1);
            ext_cell_sel.add(opt2);
            ext_cell_sel.add(opt3);
            ext_cell_sel.add(opt4);
            ext_cell_sel.addEventListener('change', this.set_cc_exterior);

            for (let k = 0; k < ext_cell_sel.options.length; k += 1) {
                const ext_cell_val = parseInt(ext_cell_sel[k].value, 10);
                if (ext_cell_val === ext_p) {
                    ext_cell_sel.selectedIndex = k;
                    break;
                }
            }
            ext_cell_sel.style.float = 'left';
            ext_cell.appendChild(ext_cell_sel);
            const primdiv = document.createElement('div');
            primdiv.textContent = 'A';
            ext_cell.appendChild(primdiv);

            const int_cell = cc_row.insertCell();
            const int_cell_sel = document.createElement('select');
            let unit;

            if (cc_type === 'A') {
                const opt5 = document.createElement('option');
                opt5.text = 25;
                const opt6 = document.createElement('option');
                opt6.text = 50;
                const opt7 = document.createElement('option');
                opt7.text = 75;
                const opt8 = document.createElement('option');
                opt8.text = 100;
                int_cell_sel.add(opt5);
                int_cell_sel.add(opt6);
                int_cell_sel.add(opt7);
                int_cell_sel.add(opt8);
                unit = 'mA';
            }
            else {
                const opt5 = document.createElement('option');
                opt5.text = 333;
                const opt6 = document.createElement('option');
                opt6.text = 1000;
                int_cell_sel.add(opt5);
                int_cell_sel.add(opt6);
                unit = 'mV'
            }

            int_cell_sel.addEventListener('change', this.set_cc_interior);

            for (let k = 0; k < int_cell_sel.options.length; k += 1) {
                const int_cell_val = parseInt(int_cell_sel[k].value, 10);
                if (int_cell_val === int_p) {
                    int_cell_sel.selectedIndex = k;
                    break;
                }
            }
            int_cell_sel.style.float = 'left';
            int_cell.appendChild(int_cell_sel);
            const unitdiv = document.createElement('div');
            unitdiv.textContent = unit;
            int_cell.appendChild(unitdiv);

            const mp_cell = cc_row.insertCell();
            mp_cell.textContent = mp;
            mp_cell.contentEditable = true;
            mp_cell.addEventListener('focusout', this.set_cc_midpoint);
        });
        await disable_interaction(false);
    }

    async set_cc_exterior(event) {
        await disable_interaction(true);
        this.extphase = event.target.parentNode.parentNode.cells[0].innerHTML;
        this.ext_val = event.srcElement.value;
        switch (this.extphase) {
            case 'CC1':
                this.extphase = 1;
                break;
            case 'CC2':
                this.extphase = 2;
                break;
            case 'CC3':
                this.extphase = 3;
                break;
            default:
                this.extphase = 'undefined';
        }
        this.interior = event.target.parentNode.parentNode.cells[2].childNodes[0].value;
        await this.dev.set_cc_gain(this.extphase, this.ext_val, this.interior);
        await disable_interaction(false);
    }

    async set_cc_interior(event) {
        await disable_interaction(true);
        this.intphase = event.target.parentNode.parentNode.cells[0].innerHTML;
        this.int_val = event.srcElement.value;
        switch (this.intphase) {
            case 'CC1':
                this.intphase = 1;
                break;
            case 'CC2':
                this.intphase = 2;
                break;
            case 'CC3':
                this.intphase = 3;
                break;
            default:
                this.intphase = 'undefined';
        }
        this.exterior = event.target.parentNode.parentNode.cells[1].childNodes[0].value;
        await this.dev.set_cc_gain(this.intphase, this.exterior, this.int_val);
        await disable_interaction(false);
    }

    async set_cc_midpoint(event) {
        await disable_interaction(true);
        this.mpphase = event.target.parentElement.cells[0].innerHTML;
        this.mp_val = event.target.innerHTML;
        this.dev.update_midpoint(this.mp_val, this.mpphase);
        await disable_interaction(false);
    }
}
