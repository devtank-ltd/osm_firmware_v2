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
    this.port.onmessage = (e) => { this.read(e.data); };
  }

  async write(msg) {
    this.port.send(msg);
  }

  async read(data) {
    this.data = data;
    return this.data;
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
      } else if (i === '}============') {
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
