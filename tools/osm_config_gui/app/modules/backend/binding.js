const END_LINE = '}============';
const START_LINE = '============{';

function on_websocket_disconnect() {
    const dialog = document.getElementById('osm-disconnect-dialog');
    const confirm = document.getElementById('osm-disconnect-confirm');
    dialog.showModal();
    const controller = new AbortController();

    confirm.addEventListener('click', async () => {
        dialog.close();
        controller.abort();
        window.location.reload();
    });
}

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
    constructor(url) {
        this.url = url;
        this.msgs = '';
        this.url.onerror = (event) => {
            console.log(event);
        };
        this.url.onclose = () => {
            on_websocket_disconnect();
        };
        this.on_message();
    }

    async write(msg) {
        this.url.send(`${msg}\n`);
    }

    async read() {
        await this.wait_for_messages();
        const msg = this.msgs;
        this.msgs = '';
        return msg;
    }

    async on_message() {
        this.url.onmessage = async (e) => {
            this.msgs += e.data;
        };
    }

    async wait_for_messages() {
        return new Promise((resolve) => {
            const check_messages = () => {
                if (this.msgs.includes(END_LINE)) {
                    this.msgs = this.msgs.replace(END_LINE, '');
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

    async read(end_line = END_LINE, timeout=this.timeout_ms) {
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
                if (msgs.includes(end_line)) {
                    msgs = msgs.replace(end_line, '');
                    break;
                }
            }
        } catch (error) {
            console.log(error);
        } finally {
            reader.releaseLock();
        }
        return msgs;
    }

    async read_raw() {
        const decoder = new TextDecoder();
        let reader, msg;
        try {
            reader = this.port.readable.getReader();
            const {value, done} = await reader.read();
            msg = decoder.decode(value);
            console.log(msg);
        } catch (error) {
            console.log(error);
        } finally {
            reader.releaseLock();
        }
        console.log(msg);
        return msg;
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
    constructor(hex, func, name, datatype) {
        this.hex = hex;
        this.func = func;
        this.name = name;
        this.datatype = datatype;
    }
}

export class binding_t {
    constructor(port, port_type) {
        this.port = port;
        this.port_type = port_type;
        this.timeout = 1000;
        this.queue = [];
        this.call_queue();
    }

    async open_ll_obj() {
        if (this.port_type === 'Serial') {
            this.ll = new low_level_serial_t(this.port, this.timeout);
        } else {
            const mod = this.port.startsWith('https://') ? `wss://${this.port.substring(8)}` : `ws://${this.port.substring(7)}`;
            this.url = new WebSocket(mod);
            const opened = await this.connection(this.url);
            if (opened) {
                this.ll = new low_level_socket_t(this.url);
            }
        }
        return this.ll;
    }

    async connection(socket, timeout = 10000) {
        const is_opened = () => (socket.readyState === WebSocket.OPEN);

        if (socket.readyState !== WebSocket.CONNECTING) {
            return is_opened();
        }

        const intrasleep = 100;
        const ttl = timeout / intrasleep;
        this.loop = 0;
        while (socket.readyState === WebSocket.CONNECTING && this.loop < ttl) {
            await new Promise((resolve) => setTimeout(resolve, intrasleep));
            this.loop += 1;
        }
        return is_opened();
    }

    async enqueue_and_process(msg) {
        this.queue.push(msg);
    }

    async process_queue() {
        while (this.queue.length > 0) {
            const msg = this.queue.shift();
            await this.do_cmd(msg);
        }
    }

    async call_queue() {
        this.process_queue();
        setTimeout(() => { this.call_queue(); }, 1000);
    }

    async parse_msg(msg) {
        const multi = await this.parse_msg_multi(msg);
        this.msgs = multi.join('');
        return this.msgs;
    }

    async parse_msg_multi(msg) {
        if (typeof (msg) !== 'string') {
            return '';
        }
        this.msgs = [];
        const spl = msg.split('\n\r');
        spl.forEach((s, i) => {
            if (s === START_LINE) {
                console.log('Found start line');
            } else if (s === END_LINE) {
                console.log('Found end line');
            } else if (s.includes('DEBUG') || s.includes('ERROR')) {
                console.log('Found debug line');
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

    async reset() {
        await this.ll.write('reset\r\n');
        let op = await this.ll.read_raw();
        return op
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
        [this.s] = this.text.split(END_LINE);
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
            if (i) {
                if (i.includes('Sample Count')) {
                    start = index;
                    m.push('Last Value');
                    measurements.push(m);
                } else if (i.includes(END_LINE)) {
                    end = index;
                } else {
                    try {
                        const [, interval_str] = m;
                        regex = /^(\d+)x(\d+(\.\d+)?)mins$/;
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
            }
        });
        const extracted_meas = measurements.slice(start, end);
        if (interval_mins < 1 && interval_mins > 0) {
            interval_mins *= 60;
            extracted_meas[0][1] = ('Uplink Time (Seconds)');
        } else {
            extracted_meas[0][1] = ('Uplink Time (Mins)');
        }
        return extracted_meas;
    }

    async enter_comms_direct() {
        let cmd = await this.ll.write('comms_direct\r\n');
        let read_until = await this.ll.read('Exiting COMMS_DIRECT mode', 5000);
        return;
    }


    async change_interval(meas, num) {
        this.int = await this.do_cmd(`interval ${meas} ${num}`);
        return this.int;
    }

    async change_samplecount(meas, num) {
        this.sample = await this.do_cmd(`samplecount ${meas} ${num}`);
        return this.sample;
    }

    async save() {
        await this.do_cmd('save');
    }

    async ios() {
        this.io_obj = await this.do_cmd_multi('ios');
        return this.io_obj;
    }

    async activate_io(meas, index, edge, pull) {
        if (meas.includes('CNT') || meas === 'PLSCNT') {
            await this.do_cmd(`en_pulse ${index} ${edge} ${pull}`);
        } else if (meas.includes('TMP' || meas === 'W1')) {
            await this.do_cmd(`en_w1 ${index} ${pull}`);
        } else if (meas.includes('IO0' || meas === 'WATCH')) {
            await this.do_cmd(`en_watch ${index} ${pull}`);
        }
    }

    async disable_io(index) {
        this.disabled = this.do_cmd(`io ${index} : I N`);
        return this.disabled;
    }

    async update_midpoint(value, phase) {
        await this.do_cmd(`cc_mp ${value} ${phase}`);
    }

    async get_cc_type(phase) {
        this.types = await this.do_cmd('cc_type');
        const regex = new RegExp(`CC${phase}\\sType:\\s([AV])`, 'g');
        const match = this.types.match(regex);
        if (match) {
            return match[1];
        }
        return this.types;
    }

    async get_cc_gain() {
        const gain = await this.do_cmd_multi('cc_gain');
        return gain;
    }

    async set_cc_gain(phase, ext, int) {
        await this.do_cmd(`cc_gain ${phase} ${ext} ${int}`);
    }

    async get_cc_mp(phase) {
        const mp = await this.do_cmd_multi(`cc_mp CC${phase}`);
        let mp_match;
        const mp_regex = /MP:\s(\d+(\.\d+)?)/;
        for (let i = 0; i < mp.length; i += 1) {
            mp_match = await mp[i].match(mp_regex);
            if (mp_match) {
                break;
            }
        }
        let mp_extracted;
        if (mp_match) {
            [, mp_extracted] = mp_match;
        } else {
            mp_extracted = '';
        }
        return mp_extracted;
    }

    async cc_cal() {
        this.calibrate = await this.do_cmd('cc_cal');
    }

    async extract_interval_mins() {
        const imins = await this.do_cmd('interval_mins');
        const regex = /\d+(\.\d+)?/g;
        const match = imins.match(regex)[0];
        return match;
    }

    async get_interval_meas(meas) {
        this.int = await this.do_cmd(`interval ${meas}`);
        const regex = /\d+/g;
        const match = this.int.match(regex)[0];
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

    async get_value(cmd) {
        const res = await this.do_cmd(cmd);
        if (res === 'Failed to get measurement reading.') {
            return 'n/a';
        }
        if (res.includes(':')) {
            const [, value] = res.split(': ');
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
                const [, , , , , h, f, n, datatype] = await curr.split(' ');
                const regname = n.replace(/"/g, '');
                const func_str = f.match(/\d+/g);
                const hex = parseInt(h.slice(2), 16);
                const func = Number(func_str);
                const reg = new modbus_register_t(hex, func, regname, datatype);
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
        this.mb_dev = await this.do_cmd(`mb_dev_add ${unit_id} ${byteorder} ${wordorder} ${name}`);
        return this.mb_dev;
    }

    async mb_reg_add(unit_id, hex, func, unit, reg) {
        this.reg = await this.do_cmd(`mb_reg_add ${unit_id} ${hex} ${func} ${unit} ${reg}`);
        return this.reg;
    }

    async remove_modbus_reg(reg) {
        this.reg_del = await this.do_cmd(`mb_reg_del ${reg}`);
        return this.reg_del;
    }

    async remove_modbus_dev(dev) {
        this.dev_del = await this.do_cmd(`mb_dev_del ${dev}`);
        return this.dev_del;
    }

    async modbus_setup(mode, baud, config) {
        this.setup = await this.do_cmd(`mb_setup ${mode} ${baud} ${config}`);
        return this.setup;
    }

    async wipe() {
        this.wipe_cmd = this.ll.write('wipe');
        await this.ll.read('Flash successfully written');
    }

    async comms_type() {
        const comms_config = await this.do_cmd('j_comms_cfg');
        const json_config = JSON.parse(comms_config);
        const type = await json_config.type;
        return type;
    }

    async get_io_types() {
        const measurements = await this.get_measurements();
        const io_list = [];
        for (let i = 1; i < measurements.length; i += 1) {
            const meas = measurements[i][0];
            this.ftma_types = await this.do_cmd(`get_meas_type ${meas}`);
            const s = this.ftma_types.split(': ');
            const io = s[0].replace(START_LINE, '');
            if (s[1] === 'IO_READING' || s[1] === 'PULSE_COUNT' || s[1] === 'W1_PROBE') {
                io_list.push(io);
            }
        }
        return io_list;
    }

    async get_ftma_types() {
        const measurements = await this.get_measurements();
        const ftma_list = [];
        for (let i = 1; i < measurements.length; i += 1) {
            const meas = measurements[i][0];
            this.ftma_types = await this.do_cmd(`get_meas_type ${meas}`);
            const s = this.ftma_types.split(': ');
            if (s[1] === 'FTMA') {
                ftma_list.push(s[0]);
            }
        }
        return ftma_list;
    }

    async get_ftma_coeffs(meas) {
        this.coeffs = await this.do_cmd_multi(`ftma_coeff ${meas}`);
        return this.coeffs;
    }

    async set_ftma_coeffs(meas, a, b, c, d) {
        await this.do_cmd(`ftma_coeff ${meas} ${a} ${b} ${c} ${d}`);
    }

    async set_ftma_name(old_name, new_name) {
        await this.do_cmd(`ftma_name ${old_name} ${new_name}`);
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

export class wifi_comms_t {
    constructor(dev) {
        this.dev = dev;
    }

    get wifi_ssid() {
        return this.dev.get_value('comms_config wifi_ssid');
    }

    set wifi_ssid(ssid) {
        this.dev.enqueue_and_process(`comms_config wifi_ssid ${ssid}`);
    }

    get wifi_pwd() {
        return this.dev.get_value('comms_config wifi_pwd');
    }

    set wifi_pwd(pwd) {
        this.dev.enqueue_and_process(`comms_config wifi_pwd ${pwd}`);
    }

    get mqtt_addr() {
        return this.dev.get_value('comms_config mqtt_addr');
    }

    set mqtt_addr(addr) {
        this.dev.enqueue_and_process(`comms_config mqtt_addr ${addr}`);
    }

    get mqtt_user() {
        return this.dev.get_value('comms_config mqtt_user');
    }

    set mqtt_user(mqtt_user) {
        this.dev.enqueue_and_process(`comms_config mqtt_user ${mqtt_user}`);
    }

    get mqtt_pwd() {
        return this.dev.get_value('comms_config mqtt_pwd');
    }

    set mqtt_pwd(mqtt_pwd) {
        this.dev.enqueue_and_process(`comms_config mqtt_pwd ${mqtt_pwd}`);
    }

    get mqtt_ca() {
        return this.dev.get_value('comms_config mqtt_ca');
    }

    set mqtt_ca(mqtt_ca) {
        this.dev.enqueue_and_process(`comms_config mqtt_ca ${mqtt_ca}`);
    }

    get mqtt_port() {
        return this.dev.get_value('comms_config mqtt_port');
    }

    set mqtt_port(mqtt_port) {
        this.dev.enqueue_and_process(`comms_config mqtt_port ${mqtt_port}`);
    }

    get comms_conn() {
        return this.dev.get_value('comms_conn');
    }
}
