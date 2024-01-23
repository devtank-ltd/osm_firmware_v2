export async function generate_random(len) {
  const chars = '0123456789ABCDEF';
  const string_length = len;
  let key = '';
  for (let i = 0; i < string_length; i += 1) {
    const rnum = Math.floor(Math.random() * chars.length);
    key += chars.substring(rnum, rnum + 1);
  }
  return key;
}

class low_level_socket_t {
  constructor(port) {
    this.port = new WebSocket(port);
    this.port.onopen = (e) => {
      console.log(`Socket opened on port ${port}`);
    };
    this.msgs = '';
    this.port.onerror = (event) => {
      console.log('WebSocket error: ', event);
    };
    this.on_message();
  }

  async write(msg) {
    this.port.send(`${msg}\n`);
  }

  async read() {
    await this.wait_for_messages();
    const msg = this.msgs;
    this.msgs = '';
    return msg;
  }

  async on_message() {
    this.port.onmessage = async (e) => {
      this.msgs += e.data;
      console.log(e.data);
    };
  }

  async wait_for_messages() {
    return new Promise((resolve) => {
      const check_messages = () => {
        if (this.msgs.includes('}============')) {
          resolve();
        } else {
          setTimeout(check_messages, 100);
        }
      };
      check_messages();
    });
  }
}

class low_level_serial_t {
  constructor(port, timeout_ms) {
    this.port = port;
    this.timeout_ms = timeout_ms;
  }

  async write(msg) {
    const encoder = new TextEncoder();
    const writer = this.port.writable.getWriter();
    await writer.write(encoder.encode(`${msg}\n`));
    writer.releaseLock();
  }

  async read() {
    const decoder = new TextDecoder();
    let msgs = '';
    const start_time = Date.now();
    const reader = this.port.readable.getReader();
    try {
      while (Date.now() > start_time - this.timeout_ms) {
        const { value, done } = await reader.read();
        if (done) {
          break;
        }
        const msg = decoder.decode(value);
        msgs += msg;
        if (msgs.includes('}============')) {
          console.log('Finished reading OSM.');
          break;
        }
      }
    } catch (error) {
      console.log(error);
    } finally {
      reader.releaseLock();
    }
    console.log(`Got ${msgs}`);
    return msgs;
  }
}

class modbus_bus_t {
  constructor(config, devices) {
    this.config = config;
    this.devices = devices;
  }
}

class modbus_device_t {
  constructor(name, unit_id, byteorder, wordorder, registers) {
    this.name = name;
    this.byteorder = byteorder;
    this.wordorder = wordorder;
    this.registers = registers;
    this.unit_id = unit_id;
  }
}

class modbus_register_t {
  constructor(hex, func, name) {
    this.hex = hex;
    this.func = func;
    this.name = name;
  }
}

export class binding_t {
  constructor(port, port_type) {
    this.port = port;
    this.port_type = port_type;
    this.timeout = 1000;
    if (this.port_type === 'Serial') {
      this.ll = new low_level_serial_t(this.port, this.timeout);
    } else {
      this.ll = new low_level_socket_t(this.port);
    }

    this.queue = [];
    this.call_queue();
  }

  async enqueue_and_process(msg) {
    this.queue.push(msg);
  }

  async process_queue() {
    while (this.queue.length > 0) {
      const msg = this.queue.shift();
      await this.do_cmd(msg);
      console.log(`Message from queue "${msg}" sent`);
    }
  }

  async call_queue() {
    this.process_queue();
    setTimeout(() => { this.call_queue(); }, 1000);
  }

  async parse_msg(msg) {
    if (typeof (msg) !== 'string') {
      console.log(typeof (msg));
      return '';
    }
    this.msgs = '';
    const spl = msg.split('\n\r');
    spl.forEach((s, i) => {
      if (s === '============{') {
        console.log('Start found');
      } else if (s === '}============') {
        console.log('End found');
      } else if (s.includes('DEBUG') || s.includes('ERROR')) {
        console.log('Ignoring debug/error msg.');
      } else {
        this.msgs += s;
      }
    });
    if (!spl) {
      return '';
    }
    return this.msgs;
  }

  async parse_msg_multi(msg) {
    if (typeof (msg) !== 'string') {
      console.log(typeof (msg));
      return '';
    }
    this.msgs = [];
    const spl = msg.split('\n\r');
    spl.forEach((s, i) => {
      if (s === '============{') {
        console.log('Start found');
      } else if (s === '}============') {
        console.log('End found');
      } else if (s.includes('DEBUG') || s.includes('ERROR')) {
        console.log('Ignoring debug/error msg.');
      } else {
        this.msgs.push(s);
      }
    });
    if (!spl) {
      return [];
    }
    return this.msgs;
  }

  async do_cmd(cmd) {
    await this.ll.write(cmd);
    const output = await this.ll.read();
    const parsed = await this.parse_msg(output);
    return parsed;
  }

  async do_cmd_raw(cmd) {
    await this.ll.write(cmd);
    const output = await this.ll.read();
    return output;
  }

  async do_cmd_multi(cmd) {
    await this.ll.write(cmd);
    const output = await this.ll.read();
    const parsed = await this.parse_msg_multi(output);
    return parsed;
  }

  async help() {
    this.raw = await this.do_cmd_raw('?');
    [, this.text] = this.raw.split('=============');
    [this.s] = this.text.split('}============');
    return this.s;
  }

  async get_measurements() {
    await this.ll.write('measurements');
    const meas = await this.ll.read();
    const measurements = [];
    const meas_split = meas.split('\n\r');
    let start; let end; let regex; let interval; let interval_mins;
    meas_split.forEach((i, index) => {
      const m = i.split(/[\t]{1,2}/g);
      if (i.includes('Sample Count')) {
        start = index;
        m.push('Last Value');
        measurements.push(m);
      } else if (i.includes('}============')) {
        end = index;
      } else {
        try {
          const [, interval_str] = m;
          regex = /^(\d+)x(\d+)mins$/;
          if (interval_str) {
            const match = interval_str.match(regex);
            [, interval, interval_mins] = match;
            m[1] = interval;
            m.push('');
          }
        } catch (error) {
          console.log(error);
        } finally {
          measurements.push(m);
        }
      }
    });
    const extracted_meas = measurements.slice(start, end);
    const imins = extracted_meas[0][1].concat(` (${interval_mins}mins)`);
    extracted_meas[0][1] = imins;
    return extracted_meas;
  }

  async save() {
    this.do_cmd('save');
  }

  async activate_io(meas, index, edge, pull) {
    let m = '';
    if (meas === 'PLSCNT') {
      m = 'pulse';
    } else if (meas === 'W1') {
      m = 'w1';
    }
    await this.do_cmd(`en_${m} ${index} ${edge} ${pull}`);
  }

  async disable_io(index) {
    this.do_cmd(`io ${index} : I N`);
  }

  async update_midpoint(value, phase) {
    await this.do_cmd(`cc_mp ${value} ${phase}`);
  }

  async extract_interval_mins() {
    const imins = await this.do_cmd('interval_mins');
    const regex = /\d+/g;
    const match = imins.match(regex)[0];
    return match;
  }

  get interval_mins() {
    return this.extract_interval_mins();
  }

  set interval_mins(value) {
    this.enqueue_and_process(`interval_mins ${value}`);
  }

  set serial_number(value) {
    this.enqueue_and_process(`serial_num ${value}`);
  }

  get serial_number() {
    return this.get_value('serial_num');
  }

  get firmware_version() {
    return this.get_value('version');
  }

  /*
  async get_wifi_config() {

            comms_pr_cfg

            WIFI SSID: Wifi Example SSID
            WIFI PWD: wifipwd
            MQTT ADDR: mqtt_addr
            MQTT USER: mqttuser
            MQTT PWD: mqttpwd
            MQTT CA: server
            MQTT PORT: 8883

  }
  */

  async get_value(cmd) {
    const res = await this.do_cmd(cmd);
    if (res === 'Failed to get measurement reading.') {
      return 'n/a';
    }
    if (res.includes(':')) {
      const [, value] = res.split(':');
      if (value.includes('"')) {
        return value.replace(/"/g, '');
      }
      return value;
    }
    return res;
  }

  async modbus_config() {
    this.mb = await this.do_cmd_multi('mb_log');
    this.conf = [];
    this.dev = {};
    this.devs = [];
    for (let line = 0; line < this.mb.length; line += 1) {
      const curr = this.mb[line];
      if (curr.includes('Modbus')) {
        const [,, ...conf] = curr.split(' ');
        this.conf = conf;
      }
      if (curr.includes('- Device')) {
        const [,,, unit_id, name, byteorder, wordorder] = curr.split(' ');
        this.dev = {
          unit_id: parseInt(unit_id.slice(2), 16),
          name: name.replace(/"/g, ''),
          byteorder,
          wordorder,
          registers: [],
        };
        this.devs.push(this.dev);
      }
      if (curr.includes('- Reg')) {
        const [, , , , , h, f, n] = await curr.split(' ');
        const regname = n.replace(/"/g, '');
        const func_str = f.match(/\d+/g);
        const hex = parseInt(h.slice(2), 16);
        const func = Number(func_str);
        const reg = new modbus_register_t(hex, func, regname);
        this.dev.registers.push(reg);
      }
    }

    this.mb_object = new modbus_bus_t(
      this.conf,
      this.devs.map((dev) => new modbus_device_t(
        dev.name,
        dev.unit_id,
        dev.byteorder,
        dev.wordorder,
        dev.registers,
      )),
    );
    return this.mb_object;
  }

  async mb_dev_add(unit_id, byteorder, wordorder, name) {
    await this.do_cmd(`mb_dev_add ${unit_id} ${byteorder} ${wordorder} ${name}`);
  }

  async mb_reg_add(unit_id, hex, func, unit, reg) {
    await this.do_cmd(`mb_reg_add ${unit_id} ${hex} ${func} ${unit} ${reg}`);
  }

  async remove_modbus_reg(reg) {
    await this.do_cmd(`mb_reg_del ${reg}`);
  }

  async remove_modbus_dev(dev) {
    await this.do_cmd(`mb_dev_del ${dev}`);
  }

  async modbus_setup(mode, baud, config) {
    await this.do_cmd(`mb_setup ${mode} ${baud} ${config}`);
  }
}

export class lora_comms_t {
  constructor(dev) {
    this.dev = dev;
  }

  get lora_deveui() {
    return this.dev.get_value('comms_config dev-eui');
  }

  set lora_deveui(deveui) {
    this.dev.enqueue_and_process(`comms_config dev-eui ${deveui}`);
  }

  get lora_appkey() {
    return this.dev.get_value('comms_config app-key');
  }

  set lora_appkey(appkey) {
    this.dev.enqueue_and_process(`comms_config app-key ${appkey}`);
  }

  get lora_region() {
    return this.dev.get_value('comms_config region');
  }

  set lora_region(region) {
    this.dev.enqueue_and_process(`comms_config region ${region}`);
  }

  get comms_conn() {
    return this.dev.get_value('comms_conn');
  }
}
