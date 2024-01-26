export class current_clamp_t {
  constructor(dev) {
    this.dev = dev;
    this.create_cc_table = this.create_cc_table.bind(this);
  }

  async open_cc_tab() {
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/current_clamp.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    this.create_cc_table();
  }

  async create_cc_table() {
    this.cc_div = document.getElementById('current-clamp-div');
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
            const mp_regex = /MP:\s(\d+(\.\d+)?)/;
            const mp_match = phase_one_mp[0].match(mp_regex);
            const mp_extracted = mp_match[1];
            measurements.push(mp_extracted);

            top_level.push(measurements);
          } else if (index <= 3) {
            const mp_regex = /MP:\s(\d+(\.\d+)?)/;
            const mp_match = phase_two_mp[0].match(mp_regex);
            const mp_extracted = mp_match[1];
            measurements.push(mp_extracted);

            top_level.push(measurements);
          } else if (index <= 5) {
            const mp_regex = /MP:\s(\d+(\.\d+)?)/;
            const mp_match = phase_three_mp[0].match(mp_regex);
            const mp_extracted = mp_match[1];
            measurements.push(mp_extracted);

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
      const int_cell = cc_row.insertCell();
      int_cell.textContent = int * 1000;
      int_cell.contentEditable = true;
      const mp_cell = cc_row.insertCell();
      mp_cell.textContent = mp;
      mp_cell.contentEditable = true;
    });
  }
}
