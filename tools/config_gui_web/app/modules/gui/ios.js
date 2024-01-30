export class io_t {
  constructor(dev) {
    this.dev = dev;
  }

  async open_io_tab() {
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/io.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    await this.create_io_table();
  }

  async create_io_table() {
    const ios = await this.get_ios();
    this.checkboxes = [];
    this.io_div = document.getElementById('io-div');
    this.add_io_table = this.io_div.appendChild(document.createElement('table'));
    this.add_io_table.id = 'io-table';
    this.add_io_table.classList = 'io-table';
    const tbody = this.add_io_table.createTBody();
    this.add_io_table.createTHead();

    const title = this.add_io_table.tHead.insertRow();
    const title_cell = title.insertCell();
    title_cell.textContent = 'IO Configuration';
    title_cell.colSpan = 4;

    const headers = this.add_io_table.tHead.insertRow();
    headers.insertCell().textContent = 'Measurement';
    headers.insertCell().textContent = 'Pull';
    headers.insertCell().textContent = 'Edge';
    headers.insertCell().textContent = 'Enabled';

    const measurements = ['TMP2', 'CNT1', 'CNT2', 'IO04', 'IO05'];

    measurements.forEach((meas) => {
      const io_row = tbody.insertRow();

      io_row.insertCell().textContent = meas;
      const pullup_cell = io_row.insertCell();
      this.pullup_cell_input = document.createElement('select');
      const pullup = document.createElement('option');
      pullup.text = 'Up';
      const pulldown = document.createElement('option');
      pulldown.text = 'Down';
      const pullnone = document.createElement('option');
      pullnone.text = 'None';
      this.pullup_cell_input.add(pullup);
      this.pullup_cell_input.add(pulldown);
      this.pullup_cell_input.add(pullnone);
      pullup_cell.appendChild(this.pullup_cell_input);

      const edge_cell = io_row.insertCell();
      this.edge_cell_input = document.createElement('select');
      const rise = document.createElement('option');
      rise.text = 'Rising';
      const fall = document.createElement('option');
      fall.text = 'Falling';
      const both = document.createElement('option');
      both.text = 'Both';
      this.edge_cell_input.add(rise);
      this.edge_cell_input.add(fall);
      this.edge_cell_input.add(both);
      edge_cell.appendChild(this.edge_cell_input);

      const chk = document.createElement('input');
      chk.type = 'checkbox';
      const chkcell = io_row.insertCell();
      chkcell.appendChild(chk);
      this.checkboxes.push(chk);
      if (meas === 'TMP2') {
        this.edge_cell_input.disabled = true;
        if (ios[4].includes('USED W1')) {
          chk.checked = true;
        }
      } else if (meas === 'CNT1') {
        if (ios[4].includes('USED PLSCNT')) {
          chk.checked = true;
        }
      } else if (meas === 'CNT2') {
        if (ios[5].includes('USED PLSCNT')) {
          chk.checked = true;
        }
      } else if (meas === 'IO04') {
        this.edge_cell_input.disabled = true;
        if (ios[4].includes('USED WATCH')) {
          chk.checked = true;
        }
      } else if (meas === 'IO05') {
        this.edge_cell_input.disabled = true;
        if (ios[5].includes('USED WATCH')) {
          chk.checked = true;
        }
      }
      chk.addEventListener('click', (e) => {
        this.enable_disable_io(e);
      });
    });
  }

  async get_ios() {
    this.ios = await this.dev.ios();
    return this.ios;
  }

  async enable_disable_io(e) {
    this.checked = e.target.checked;
    this.index = 4;
    this.meas = e.target.parentNode.parentNode.cells[0].innerHTML;
    if (this.meas === 'IO05') {
      this.index = 5;
    }
    if (this.meas === 'CNT2') {
      this.index = 5;
    }
    [this.pullup] = e.target.parentNode.parentNode.cells[1].childNodes[0].value;
    [this.edge] = e.target.parentNode.parentNode.cells[2].childNodes[0].value;
    if (this.checked === true) {
      await this.dev.activate_io(this.meas, this.index, this.edge, this.pullup);
    } else {
      await this.dev.disable_io(this.index);
    }
    if (this.meas === 'TMP2' && this.checked === true) {
      this.checkboxes[1].checked = false;
      this.checkboxes[3].checked = false;
    }
    if (this.meas === 'CNT1' && this.checked === true) {
      this.checkboxes[0].checked = false;
      this.checkboxes[3].checked = false;
    }
    if (this.meas === 'IO04' && this.checked === true) {
      this.checkboxes[0].checked = false;
      this.checkboxes[1].checked = false;
    }
    if (this.meas === 'CNT2' && this.checked === true) {
      this.checkboxes[4].checked = false;
    }
    if (this.meas === 'IO05' && this.checked === true) {
      this.checkboxes[2].checked = false;
    }
  }
}
