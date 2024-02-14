import { disable_interaction } from './disable.js';

export class measurements_table_t {
  constructor(dev) {
    this.dev = dev;
    this.set_interval_or_samplec = this.set_interval_or_samplec.bind(this);
    this.insert_last_value = this.insert_last_value.bind(this);
    this.change_uplink_time = this.change_uplink_time.bind(this);
  }

  async create_measurements_table_gui() {
    await disable_interaction(true);
    const measurements = await this.dev.get_measurements();
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
          row.insertCell().textContent = c;
        });
      } else {
        const r = tBody.insertRow();
        cell.forEach((j, index) => {
          const entry = r.insertCell();
          if (index === 0) {
            entry.textContent = j;
          }
          if (index === 1 || index === 2) {
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
            entry.addEventListener('focusout', this.set_interval_or_samplec);
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
    const uplink_submit = document.getElementById('home-uplink-submit');
    uplink_submit.onclick = this.change_uplink_time;
  }

  async get_minimum_interval(val) {
    const interval_mins = await this.dev.interval_mins;
    let interval_seconds = null;
    if (interval_mins < 1 && interval_mins > 0) {
      interval_seconds = interval_mins * 60;
    }
    let m;
    const multiple = 5;
    if (parseFloat(val) < interval_mins && parseFloat(val) > 0) {
      m = interval_mins;
    } else {
      m = val;
    }
    if (m % 5 !== 0) {
      m = await this.round_to_nearest(m, multiple);
    }
    let int_mins_converted = m / interval_mins;
    if (interval_seconds != null) {
      int_mins_converted = m / interval_mins / 60;
    }
    int_mins_converted = Math.round(int_mins_converted);
    return int_mins_converted;
  }

  async round_to_nearest(val, multiple) {
    this.val = val;
    return Math.round(this.val / multiple) * multiple;
  }

  async set_interval_or_samplec(e) {
    await disable_interaction(true);
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
    } else if (val < 0) {
      val = 0;
    }
    const type = table.rows[0].cells[cell_index].innerHTML;

    /* Example: interval CNT1 1 */
    if (type.includes('Uplink')) {
      const int_mins_converted = await this.get_minimum_interval(val);
      val_col.value = int_mins_converted * interval_mins;
      this.dev.enqueue_and_process(`interval ${meas} ${int_mins_converted}`);
    } else {
      val_col.value = val;
      this.dev.enqueue_and_process(`samplecount ${meas} ${val}`);
    }
    await disable_interaction(false);
  }

  async insert_last_value(e) {
    await disable_interaction(true);
    const last_val_index = 3;
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

  async change_uplink_time() {
    await disable_interaction(true);
    const table = document.getElementById('meas-table');
    const imins_header = table.rows[0].cells[1];
    const interval_mins = await this.dev.interval_mins;

    const uplink_input = document.getElementById('home-uplink-input');
    const mins = uplink_input.value;
    this.dev.interval_mins = mins;
    let seconds = null;
    if (mins < 1 && mins > 0) {
      seconds = mins * 60;
      this.is_seconds = true;
      if (seconds != null) {
        imins_header.innerHTML = `Uplink Time (${seconds} seconds)`;
      }
    } else {
      imins_header.innerHTML = `Uplink Time (${mins} mins)`;
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
