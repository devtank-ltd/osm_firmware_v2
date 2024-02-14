import { disable_interaction } from './disable.js';

export class io_t {
  constructor(dev) {
    this.dev = dev;
  }

  async open_io() {
    await disable_interaction(true);
    await this.create_io_table();
    await disable_interaction(false);
  }

  async get_ios_measurements() {
    const io_types = await this.dev.get_io_types();
    return io_types;
  }

  async get_indexs() {
    const ios = await this.get_ios();
    this.indexs = {};
    const ios_regex = /\IO (?<io>[0-9]{2}) : +(\[(?<specials_avail>[A-Za-z0-9 \|]+)\])? ((USED (?<special_used>[A-Za-z0-9]+)( (?<edge>F|R|B))?)|(?<dir>IN|OUT)) (?<pupd>DOWN|UP|NONE|D|U|N)( = (?<level>ON|OFF))?/;

    for (let i = 0; i < ios.length; i += 1) {
      const m = await ios[i].match(ios_regex);
      if (!m) {
        console.log('Invalid IOS configuration.');
        break;
      }
      const { io } = await m.groups;
      const { specials_avail } = await m.groups;
      const { special_used } = await m.groups;
      const { edge } = await m.groups;
      const { pupd } = await m.groups;
      const io_p = io[1];

      if (specials_avail) {
        this.indexs[io_p] = { specials_avail };
      }
      if (special_used) {
        this.indexs[io_p].used = special_used;
        this.indexs[io_p].edge = edge;
        this.indexs[io_p].pullup = pupd;
        this.indexs[io_p].index = io_p;
        this.indexs[io_p].avail_meas = [];
      }
    }
    return this.indexs;
  }

  async create_io_table() {
    await disable_interaction(true);
    const indexs = await this.get_indexs();
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

    const measurements = await this.get_ios_measurements();

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

      let key; let
        value;
      switch (meas) {
        case 'TMP2':
          [key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          this.edge_cell_input.disabled = true;
          if (value.used && value.used === 'W1') {
            chk.checked = true;
            if (value.edge) {
              this.get_edge(value.edge);
            }
          }
          break;
        case 'TMP3':
          [, key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          this.edge_cell_input.disabled = true;
          if (value.used && value.used === 'W1') {
            chk.checked = true;
            if (value.edge) {
              this.get_edge(value.edge);
            }
          }
          break;
        case 'CNT1':
          [key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          if (value.used && value.used === 'PLSCNT') {
            chk.checked = true;
            if (value.edge) {
              this.get_edge(value.edge);
            }
          }
          if (value.pullup) {
            this.get_pullup(value.pullup);
          }
          break;
        case 'CNT2':
          [, key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          if (value.used && value.used === 'PLSCNT') {
            chk.checked = true;
            if (value.edge) {
              this.get_edge(value.edge);
            }
          }
          if (value.pullup) {
            this.get_pullup(value.pullup);
          }
          break;
        case 'IO01':
          [key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          this.edge_cell_input.disabled = true;
          if (value.used && value.used === 'WATCH') {
            chk.checked = true;
            if (value.pullup) {
              this.get_pullup(value.pullup);
            }
          }
          break;
        case 'IO02':
          [, key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          this.edge_cell_input.disabled = true;
          if (value.used && value.used === 'WATCH') {
            chk.checked = true;
            if (value.pullup) {
              this.get_pullup(value.pullup);
            }
          }
          break;
        case 'IO04':
          [key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          this.edge_cell_input.disabled = true;
          if (value.used && value.used === 'WATCH') {
            chk.checked = true;
            if (value.pullup) {
              this.get_pullup(value.pullup);
            }
          }
          break;
        case 'IO05':
          [, key] = Object.keys(indexs);
          value = indexs[key];
          value.avail_meas.push(meas);
          this.edge_cell_input.disabled = true;
          if (value.used && value.used === 'WATCH') {
            chk.checked = true;
            if (value.pullup) {
              this.get_pullup(value.pullup);
            }
          }
          break;
        default:
          break;
      }

      chk.addEventListener('click', (e) => {
        this.enable_disable_io(e);
      });
    });
    await disable_interaction(false);
  }

  async get_pullup(pullup) {
    if (pullup === 'U') {
      this.pullup_cell_input.selectedIndex = 0;
    } else if (pullup === 'D') {
      this.pullup_cell_input.selectedIndex = 1;
    } else if (pullup === 'N') {
      this.pullup_cell_input.selectedIndex = 2;
    }
  }

  async get_edge(edge) {
    if (edge === 'R') {
      this.edge_cell_input.selectedIndex = 0;
    } else if (edge === 'F') {
      this.edge_cell_input.selectedIndex = 1;
    } else if (edge === 'B') {
      this.edge_cell_input.selectedIndex = 2;
    }
  }

  async get_ios() {
    this.ios = await this.dev.ios();
    return this.ios;
  }

  async enable_disable_io(e) {
    await disable_interaction(true);
    let index;
    this.checked = e.target.checked;
    this.meas = e.target.parentNode.parentNode.cells[0].innerHTML;
    for (const [key, value] of Object.entries(this.indexs)) {
      if (value.avail_meas.includes(this.meas)) {
        index = key;
      }
    }
    [this.pullup] = e.target.parentNode.parentNode.cells[1].childNodes[0].value;
    [this.edge] = e.target.parentNode.parentNode.cells[2].childNodes[0].value;
    if (this.checked === true) {
      await this.dev.activate_io(this.meas, index, this.edge, this.pullup);
    } else {
      await this.dev.disable_io(index);
    }
    if (this.meas === 'TMP2' && this.checked === true) {
      this.checkboxes[2].checked = false;
      this.checkboxes[4].checked = false;
    }
    if (this.meas === 'TMP3' && this.checked === true) {
      this.checkboxes[3].checked = false;
      this.checkboxes[5].checked = false;
    }
    if (this.meas === 'CNT1' && this.checked === true) {
      this.checkboxes[0].checked = false;
      this.checkboxes[4].checked = false;
    }
    if (this.meas === 'CNT2' && this.checked === true) {
      this.checkboxes[1].checked = false;
      this.checkboxes[5].checked = false;
    }
    if (this.meas === 'IO01' && this.checked === true) {
      this.checkboxes[0].checked = false;
      this.checkboxes[2].checked = false;
    }
    if (this.meas === 'IO02' && this.checked === true) {
      this.checkboxes[1].checked = false;
      this.checkboxes[3].checked = false;
    }
    if (this.meas === 'IO04' && this.checked === true) {
      this.checkboxes[0].checked = false;
      this.checkboxes[2].checked = false;
    }
    if (this.meas === 'IO05' && this.checked === true) {
      this.checkboxes[1].checked = false;
      this.checkboxes[3].checked = false;
    }
    await disable_interaction(false);
  }
}
