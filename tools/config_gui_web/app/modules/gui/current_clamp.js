export class current_clamp_t {
  constructor(dev) {
    this.dev = dev;
    this.create_cc_table = this.create_cc_table.bind(this);
    this.open_cc_modal = this.open_cc_modal.bind(this);
    this.set_cc_exterior = this.set_cc_exterior.bind(this);
    this.set_cc_interior = this.set_cc_interior.bind(this);
    this.set_cc_midpoint = this.set_cc_midpoint.bind(this);
  }

  async open_cc_tab() {
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/current_clamp.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    await this.create_cc_table();
    await this.add_event_listeners();
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
      console.log('Calibrating CC');
      await this.dev.cc_cal();
      this.create_cc_table();
    });
    this.cancel.addEventListener('click', () => {
      this.dialog.close();
    });
  }

  async create_cc_table() {
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

    const title = this.add_cc_table.tHead.insertRow();
    const title_cell = title.insertCell();
    title_cell.textContent = 'Current Clamp Configuration';
    title_cell.colSpan = 4;

    const headers = this.add_cc_table.tHead.insertRow();
    headers.insertCell().textContent = 'Measurement';
    headers.insertCell().textContent = 'Exterior (A)';
    headers.insertCell().textContent = 'Interior (mV)';
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

    top_level.forEach((i) => {
      const cc_row = tbody.insertRow();
      const [meas, ext, int, mp] = i;
      cc_row.insertCell().textContent = meas;
      const ext_cell = cc_row.insertCell();
      ext_cell.textContent = ext;
      ext_cell.contentEditable = true;
      ext_cell.addEventListener('focusout', this.set_cc_exterior);

      const int_cell = cc_row.insertCell();
      int_cell.textContent = int * 1000;
      int_cell.contentEditable = true;
      int_cell.addEventListener('focusout', this.set_cc_interior);

      const mp_cell = cc_row.insertCell();
      mp_cell.textContent = mp;
      mp_cell.contentEditable = true;
      mp_cell.addEventListener('focusout', this.set_cc_midpoint);
    });
  }

  async set_cc_exterior(event) {
    this.extphase = event.target.parentElement.cells[0].innerHTML;
    this.ext_val = event.target.innerHTML;
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
    this.interior = event.target.parentElement.cells[2].innerHTML;
    await this.dev.set_cc_gain(this.extphase, this.ext_val, this.interior);
  }

  async set_cc_interior(event) {
    this.intphase = event.target.parentElement.cells[0].innerHTML;
    this.int_val = event.target.innerHTML;
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
    this.exterior = event.target.parentElement.cells[1].innerHTML;
    await this.dev.set_cc_gain(this.intphase, this.exterior, this.int_val);
  }

  async set_cc_midpoint(event) {
    this.mpphase = event.target.parentElement.cells[0].innerHTML;
    this.mp_val = event.target.innerHTML;
    this.dev.update_midpoint(this.mp_val, this.mpphase);
  }
}
