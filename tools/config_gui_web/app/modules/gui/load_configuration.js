export class load_configuration_t {
  constructor(dev) {
    this.dev = dev;
    this.btn = document.getElementById('home-load-osm-config');
    this.open_file_dialog = this.open_file_dialog.bind(this);
  }

  async add_listener() {
    this.btn.addEventListener('click', this.open_file_dialog);
  }

  async load_gui_with_config(content) {
    this.content = JSON.parse(content);
    console.log(this.content);
    this.serial_num = this.content.serial_num;
    this.fw_ver = this.content.version;
    this.modbus_setup = this.content.modbus_bus.setup;
    this.modbus_devices = this.content.modbus_bus.modbus_devices;
    console.log(this.serial_num);
    console.log(this.fw_ver);
    console.log(this.modbus_setup);
    console.log(this.modbus_devices);
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
