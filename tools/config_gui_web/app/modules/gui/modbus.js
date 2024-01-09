export class modbus_t {
  constructor(dev) {
    this.dev = dev;
    this.load_modbus_config = this.load_modbus_config.bind(this);
  }

  async open_modbus() {
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/modbus.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    this.load_modbus_config();
  }

  async load_modbus_config() {
    this.mb_bus = await this.dev.modbus_config();
    this.current_modbus_config_table();
  }

  async current_modbus_config_table() {
    this.mb_bus = await this.dev.modbus_config();
    this.div = document.getElementById('modbus-current-config');
    const tbl = this.div.appendChild(document.createElement('table'));
    tbl.id = 'modbus-current-config-table';
    const tbody = tbl.createTBody();
    tbl.createTHead();

    const title = tbl.tHead.insertRow();
    const ti_cell = title.insertCell();
    ti_cell.colSpan = 4;
    ti_cell.textContent = 'Current Modbus Configuration';

    /* Insert headers */
    const row = tbl.tHead.insertRow();
    row.insertCell().textContent = 'Device';
    row.insertCell().textContent = 'Address';
    row.insertCell().textContent = 'Function';
    row.insertCell().textContent = 'Name';

    this.mb_bus.devices.forEach((cell, i) => {
      console.log(cell);
      cell.registers.forEach((j, index) => {
        const r = tbody.insertRow();

        r.insertCell().textContent = cell.name;
        r.insertCell().textContent = j.hex;
        r.insertCell().textContent = j.func;
        r.insertCell().textContent = j.name;
      });
    });
  }
}
