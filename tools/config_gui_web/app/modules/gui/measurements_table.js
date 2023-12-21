export class measurements_table_t {
  constructor(dev) {
    this.dev = dev;
    this.update_measurement_config = this.update_measurement_config.bind(this);
    this.insert_last_value = this.insert_last_value.bind(this);
    this.change_uplink_time = this.change_uplink_time.bind(this);
  }

  async create_measurements_table_gui() {
    const measurements = await this.dev.get_measurements();
    const res = document.querySelector('div.measurements-table');
    const tbl = res.appendChild(document.createElement('table'));
    tbl.id = 'meas-table';
    const tBody = tbl.createTBody();
    tbl.createTHead();
    this.interval_mins = await this.dev.interval_mins;

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
              val = Number(this.interval_mins) * Number(j);
            } else {
              val = j;
            }

            entry.appendChild(num_input);
            num_input.value = val;
            entry.addEventListener('focusout', this.update_measurement_config);
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
  }

  async add_uplink_listener() {
    const uplink_submit = document.getElementById('home-uplink-submit');
    uplink_submit.onclick = this.change_uplink_time;
  }

  async get_minimum_interval(val) {
    const interval_mins = await this.dev.interval_mins;
    let m;
    const multiple = 5;
    // let imins = await self.dev.interval_mins;
    if (Number(val) < interval_mins) {
      m = interval_mins;
    } else {
      m = val;
    }
    if (m % 5 !== 0) {
      m = await this.round_to_nearest(m, multiple);
    }
    let int_mins_converted = m / interval_mins;
    int_mins_converted = Math.round(int_mins_converted);
    return int_mins_converted;
  }

  async round_to_nearest(val, multiple) {
    this.val = val;
    return Math.round(this.val / multiple) * multiple;
  }

  async update_measurement_config(e) {
    const interval_mins = await this.dev.interval_mins;
    const cell_index = e.srcElement.parentNode.cellIndex;
    const row_index = e.target.parentNode.parentNode.rowIndex;
    const table = e.target.offsetParent.offsetParent;
    const meas = table.rows[row_index].cells[0].innerHTML;
    const val_col = table.rows[row_index].cells[cell_index].childNodes[0];
    const val = val_col.value;
    const type = table.rows[0].cells[cell_index].innerHTML;

    /* Example: interval CNT1 1 */
    if (type.includes('Interval')) {
      const int_mins_converted = await this.get_minimum_interval(val);
      val_col.value = int_mins_converted * interval_mins;
      this.dev.enqueue_and_process(`interval ${meas} ${int_mins_converted}`);
    } else {
      this.dev.enqueue_and_process(`samplecount ${meas} ${val}`);
    }
  }

  async insert_last_value(e) {
    const last_val_index = 3;
    const checkbox = e.target;
    const table = checkbox.offsetParent.offsetParent;
    document.getElementById('home-page-fieldset').disabled = true;
    const row_index = e.srcElement.parentElement.parentNode.rowIndex;
    const meas = table.rows[row_index].cells[0].innerHTML;
    let val = await this.dev.get_value(meas);
    val = Number(val);
    const last_val_col = table.rows[row_index].cells[last_val_index];
    last_val_col.textContent = val;
    checkbox.checked = false;
    document.getElementById('home-page-fieldset').disabled = false;
  }

  async change_uplink_time() {
    const table = document.getElementById('meas-table');
    const imins_header = table.rows[0].cells[1];
    const interval_mins = await this.dev.interval_mins;

    const uplink_input = document.getElementById('home-uplink-input');
    const mins = await this.round_to_nearest(uplink_input.value, 5);
    if (mins) {
      this.dev.interval_mins = mins;
      imins_header.innerHTML = `Interval (${mins} mins)`;
      uplink_input.value = '';
    }
    for (let i = 1; i < table.rows.length; i += 1) {
      console.log(table.rows[i].cells[1].value);
      const interval_val = table.rows[i].cells[1].childNodes[0].value;
      const interval_cell = table.rows[i].cells[1].childNodes[0];
      interval_cell.value = (interval_val * mins) / interval_mins;
    }
  }
}
