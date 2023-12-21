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
  constructor(port) {
    this.port = port;
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
    const timeout_ms = 1000;
    const reader = this.port.readable.getReader();
    try {
      while (Date.now() > start_time - timeout_ms) {
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
  constructor(port, type) {
    this.port = port;
    this.type = type;
    if (this.type === 'Serial') {
      this.ll = new low_level_serial_t(this.port);
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
    const spl = msg.split('\n\r');
    spl.forEach((s, i) => {
      if (s === '============{') {
        this.start = i + 1;
      } else if (s === '}============') {
        this.end = i;
      }
      if (s.includes('DEBUG') || s.includes('ERROR')) {
        this.start += 1;
      }
    });
    if (!spl) {
      console.log('in here');
      return '';
    }
    const sliced = spl.slice(this.start, this.end)[0];
    if (sliced) {
      const sl = sliced.split(': ');
      const r = sl[sl.length - 1];
      return r;
    }
    return '';
  }

  async do_cmd(cmd) {
    await this.ll.write(cmd);
    const output = await this.ll.read();
    const parsed = await this.parse_msg(output);
    return parsed;
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
          const [, interval_str,,] = m;
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

  get lora_deveui() {
    return this.do_cmd('comms_config dev-eui');
  }

  set lora_deveui(deveui) {
    this.enqueue_and_process(`comms_config dev-eui ${deveui}`);
  }

  get lora_appkey() {
    return this.do_cmd('comms_config app-key');
  }

  set lora_appkey(appkey) {
    this.enqueue_and_process(`comms_config app-key ${appkey}`);
  }

  get lora_region() {
    return this.do_cmd('comms_config region');
  }

  set lora_region(region) {
    this.enqueue_and_process(`comms_config region ${region}`);
  }

  get interval_mins() {
    return this.do_cmd('interval_mins');
  }

  set interval_mins(value) {
    this.enqueue_and_process(`interval_mins ${value}`);
  }

  get comms_conn() {
    return this.do_cmd('comms_conn');
  }

  set serial_number(value) {
    this.enqueue_and_process(`serial_num ${value}`);
  }

  get serial_number() {
    return this.do_cmd('serial_num');
  }

  get firmware_version() {
    return this.do_cmd('version');
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

  async get_value(meas) {
    const cmd = `get_meas ${meas}`;
    const res = await this.do_cmd(cmd);
    return res;
  }
}
