import { disable_interaction } from './disable.js';

export class measurements_table_t {
    constructor(dev) {
        this.dev = dev;
        this.set_interval = this.set_interval.bind(this);
        this.insert_last_value = this.insert_last_value.bind(this);
        this.change_uplink_time = this.change_uplink_time.bind(this);
        this.desc_map = {
            "PM1"  : "Air Quality",
            "PM4"  : "Air Quality",
            "PM10" : "Air Quality",
            "PM25" : "Air Quality",
            "CC1"  : "Phase 1 Current",
            "CC2"  : "Phase 2 Current",
            "CC3"  : "Phase 3 Current",
            "TMP2" : "One Wire Probe Temperature",
            "TMP3" : "One Wire Probe Temperature",
            "TMP5" : "Temperature (Particulate Sensor)",
            "TEMP" : "Temperature",
            "HUM2" : "Humidity (Particulate Sensor)",
            "HUMI" : "Humidity",
            "BAT"  : "Battery",
            "CNT1" : "Pulsecount",
            "CNT2" : "Pulsecount",
            "LGHT" : "Light",
            "SND"  : "Sound",
            "IO01" : "Pulsecount",
            "IO02" : "Pulsecount",
        }
    }

    async create_measurements_table_gui() {
        await disable_interaction(true);
        const measurements = await this.dev.get_measurements();
        measurements.splice(1, 2); /* Removes FW and CREV from measurements */
        const res = document.querySelector('div.measurements-table');
        const tbl = res.appendChild(document.createElement('table'));
        tbl.id = 'meas-table';
        const tBody = tbl.createTBody();
        tbl.createTHead();
        this.interval_mins = await this.dev.interval_mins;
        if (this.interval_mins < 1 && this.interval_mins > 0) {
            this.interval_mins *= 60;
            this.is_seconds = true;
        } else {
            this.is_seconds = false;
        }

        measurements.forEach((cell, i) => {
            if (i === 0) {
                /* Insert headers */
                const row = tbl.tHead.insertRow();
                cell.forEach((c) => {
                    if (c !== 'Sample Count') {
                        row.insertCell().textContent = c;
                    }
                });
            } else {
                const r = tBody.insertRow();
                cell.forEach((j, index) => {
                    let entry;
                    if (index !== 2) {
                        entry = r.insertCell();
                    }
                    if (index === 0) {
                        entry.textContent = j;
                        entry.title = this.desc_map[j];
                    }
                    if (index === 1) {
                        /* If entry is interval or sample count, user can edit the cell */
                        let val;
                        const num_input = document.createElement('input');
                        num_input.type = 'number';
                        num_input.className = 'meas-table-inputs';
                        num_input.min = 0;
                        if (index === 1) {
                            val = parseFloat(this.interval_mins) * parseFloat(j);
                        } else {
                            val = j;
                        }

                        entry.appendChild(num_input);
                        num_input.value = val;
                        entry.addEventListener('focusout', this.set_interval);
                    }
                    if (index === 3) {
                        const chk = document.createElement('input');
                        chk.type = 'checkbox';
                        const chkcell = r.insertCell();
                        chkcell.appendChild(chk);
                        chk.addEventListener('click', this.insert_last_value);
                    }
                });
            }
        });
        await disable_interaction(false);
    }

    async add_uplink_listener() {
        document.getElementById('home-uplink-submit').addEventListener('click', this.change_uplink_time);
        document.getElementById('home-uplink-input').addEventListener('keyup', this.change_uplink_time);
    }

    async get_lowest_multiple(number) {
        let multiples = [];
        for (let i = 1; i <= number; i++) {
                if (number % i === 0) {
                        multiples.push(i);
                }
        }
        if (multiples.length >= 2) {
            return multiples[1];
        } else {
                return 1;
        }
}

    async get_minimum_interval(val) {
        const errordiv = document.getElementById('home-uplink-error-div');
        const msgdiv = document.getElementById('home-uplink-msg-div');
        errordiv.textContent = '';
        msgdiv.textContent = '';
        let duration = await this.dev.interval_mins;
        if (duration < 1 && duration > 0) {
            duration *= 60;
        }
        let minimum_val;
        let multiple;

        multiple = await this.get_lowest_multiple(duration);
        if (parseFloat(val) < duration && parseFloat(val) > 0) {
            errordiv.textContent = `Rounded value entered to the minimum uplink (${duration})`;
            minimum_val = duration;
        } else {
            minimum_val = val;
        }
        if (minimum_val % multiple !== 0) {
            errordiv.textContent = `Rounded value entered to nearest multiple of the minimum uplink (${duration})`;
            minimum_val = await this.round_to_nearest(minimum_val, multiple);
        }
        let int_mins_converted = minimum_val / duration;
        int_mins_converted = Math.round(int_mins_converted);
        return int_mins_converted;
    }

    async round_to_nearest(val, multiple) {
        this.val = val;
        return Math.round(this.val / multiple) * multiple;
    }

    async set_interval(e) {
        await disable_interaction(true);
        const errordiv = document.getElementById('home-uplink-error-div');
        let interval_mins = await this.dev.interval_mins;
        if (interval_mins < 1 && interval_mins > 0) {
            interval_mins *= 60;
        }
        const cell_index = e.srcElement.parentNode.cellIndex;
        const row_index = e.target.parentNode.parentNode.rowIndex;
        const table = e.target.offsetParent.offsetParent;
        const meas = table.rows[row_index].cells[0].innerHTML;
        const val_col = table.rows[row_index].cells[cell_index].childNodes[0];
        let val = val_col.value;
        if (val > 999) {
            val = 999;
            errordiv.textContent = 'Uplink time cannot be over 999.'
        } else if (val < 0) {
            val = 0;
            errordiv.textContent = 'Uplink time cannot be less than zero.'
        }

        /* Example: interval CNT1 1 */
        const int_mins_converted = await this.get_minimum_interval(val);
        val_col.value = int_mins_converted * interval_mins;
        this.dev.enqueue_and_process(`interval ${meas} ${int_mins_converted}`);
        await disable_interaction(false);
    }

    async insert_last_value(e) {
        await disable_interaction(true);
        const last_val_index = 2;
        const checkbox = e.target;
        const table = checkbox.offsetParent.offsetParent;
        const row_index = e.srcElement.parentElement.parentNode.rowIndex;
        const meas = table.rows[row_index].cells[0].innerHTML;
        const val = await this.dev.get_value(`get_meas ${meas}`);
        const last_val_col = table.rows[row_index].cells[last_val_index];
        last_val_col.textContent = val;
        checkbox.checked = false;
        await disable_interaction(false);
    }

    async change_uplink_time(e) {
        if (e.key === 'Enter' || e.pointerType === 'mouse' || e.type === 'click') {
            await disable_interaction(true);
            let min_uplink = 0;
            const comms_type = await this.dev.comms_type();
            if (comms_type.includes('LW')) {
                min_uplink = 1;
            }
            else {
                min_uplink = 0;
            }

            const errordiv = document.getElementById('home-uplink-error-div');
            const msgdiv = document.getElementById('home-uplink-msg-div');
            errordiv.textContent = '';
            msgdiv.textContent = '';
            const table = document.getElementById('meas-table');
            const imins_header = table.rows[0].cells[1];
            const interval_mins = await this.dev.interval_mins;

            const uplink_input = document.getElementById('home-uplink-input');
            let mins = uplink_input.value;
            if (mins > 999) {
                mins = 999
                errordiv.textContent = 'Max uplink time is 999';
            }
            if (mins < min_uplink) {
                mins = 1
                errordiv.textContent = `Uplink cannot be less than ${min_uplink}`;
            }

            this.dev.interval_mins = mins;
            let seconds = null;
            if (mins < 1 && mins > 0) {
                seconds = mins * 60;
                this.is_seconds = true;
                if (seconds != null) {
                    imins_header.innerHTML = 'Uplink Time (Seconds)';
                    msgdiv.textContent = `Minimum uplink time set to ${seconds} seconds`;
                }
            } else {
                imins_header.innerHTML = 'Uplink Time (Mins)';
                msgdiv.textContent = `Minimum uplink time set to ${mins} minutes`;
            }
            uplink_input.value = '';
            for (let i = 1; i < table.rows.length; i += 1) {
                const interval_val = table.rows[i].cells[1].childNodes[0].value;
                const interval_cell = table.rows[i].cells[1].childNodes[0];
                if (seconds != null) {
                    interval_cell.value = ((interval_val * mins) / interval_mins) * 60;
                } else if (this.is_seconds === true) {
                    interval_cell.value = ((interval_val * mins) / interval_mins) / 60;
                } else {
                    interval_cell.value = ((interval_val * mins) / interval_mins);
                }
            }
            if (seconds === null) {
                this.is_seconds = false;
            }
            await disable_interaction(false);
        }
    }
}
