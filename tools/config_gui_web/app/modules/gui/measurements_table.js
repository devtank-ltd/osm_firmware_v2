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
          entry.textContent = j;
          if (index === 1 || index === 2) {
            /* If entry is interval or sample count, user can edit the cell */
            entry.contentEditable = true;
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

  async update_measurement_config(e) {
    const cell_index = e.srcElement.cellIndex;
    const row_index = e.target.parentNode.rowIndex;
    const table = e.target.offsetParent;

    const meas = table.rows[row_index].cells[0].innerHTML;
    const new_val = table.rows[row_index].cells[cell_index].innerHTML;
    const type = table.rows[0].cells[cell_index].innerHTML;

    /* Example: interval CNT1 1 */
    if (type.includes('Interval')) {
      this.dev.enqueue_and_process(`interval ${meas} ${new_val}`);
    } else {
      this.dev.enqueue_and_process(`samplecount ${meas} ${new_val}`);
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

    const uplink_input = document.getElementById('home-uplink-input');
    const mins = uplink_input.value;
    if (mins) {
      this.dev.interval_mins = mins;
      imins_header.innerHTML = `Interval (${mins} mins)`;
      uplink_input.value = '';
    }
  }
}
