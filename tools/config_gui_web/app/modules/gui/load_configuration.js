import { disable_interaction } from './disable.js';

export class load_configuration_t {
  constructor(dev, comms) {
    this.dev = dev;
    this.comms = comms;
    this.btn = document.getElementById('global-load-osm-config');
    this.open_file_dialog = this.open_file_dialog.bind(this);
    this.write_modbus = this.write_modbus.bind(this);
    this.load_gui_with_config = this.load_gui_with_config.bind(this);
  }

  async add_listener() {
    this.btn.addEventListener('click', this.open_file_dialog);
  }

  async load_gui_with_config(content) {
    disable_interaction(true);
    this.content = JSON.parse(content);
    this.serial_num = this.content.serial_num;
    this.dev.serial_number = this.serial_num;

    this.modbus_setup = this.content.modbus_bus.setup;
    this.modbus_devices = this.content.modbus_bus.modbus_devices;
    await this.dev.modbus_setup(
      this.modbus_setup[0],
      this.modbus_setup[1],
      this.modbus_setup[2],
    );
    await this.write_modbus(this.modbus_devices);

    this.measurements = this.content.measurements;
    for (const [key, value] of Object.entries(this.measurements)) {
      await this.dev.do_cmd(`interval ${key} ${value.interval}`);
      await this.dev.do_cmd(`samplecount ${key} ${value.samplecount}`);
    }

    this.ios = this.content.ios;
    if (this.ios) {
      for (const [key, value] of Object.entries(this.ios)) {
        if (value.special) {
          await this.dev.activate_io(value.special, key, value.edge, value.pull);
        } else {
          await this.dev.disable_io(key);
        }
      }
    }

    this.interval_mins = this.content.interval_mins;
    this.dev.interval_mins = this.interval_mins;

    this.app_key = this.content.app_key;
    this.dev_eui = this.content.dev_eui;
    if (this.app_key) {
      this.comms.lora_appkey = this.app_key;
      this.comms.lora_deveui = this.dev_eui;
    }

    this.cc_midpoints = this.content.cc_midpoints;
    for (const [key, value] of Object.entries(this.cc_midpoints)) {
      await this.dev.update_midpoint(key, value);
    }
    disable_interaction(false);
  }

  async write_modbus(mb_devs) {
    mb_devs.forEach((i) => {
      this.dev.mb_dev_add(i.unit, i.byteorder, i.wordorder, i.name);
      i.registers.forEach((e) => {
        this.dev.mb_reg_add(i.unit, e.address, e.type, e.function, e.reg);
      });
    });
  }

  async open_file_dialog() {
    this.load_file_dialog = document.createElement('input');
    this.load_file_dialog.type = 'file';
    this.load_file_dialog.accept = '.json';
    this.load_file_dialog.onchange = (_) => {
      const reader = new FileReader();
      const file = Array.from(this.load_file_dialog.files)[0];
      if (file.name.includes('.json')) {
        reader.readAsText(file, 'application/json');
        reader.onload = (e) => { this.load_gui_with_config(e.target.result); };
        reader.onerror = (error) => {
          console.log(`Error: ${error}`);
        };
      } else {
        console.log(`Unsupported filetype '${file.type}' selected.`);
      }
    };
    this.load_file_dialog.click();
  }
}
