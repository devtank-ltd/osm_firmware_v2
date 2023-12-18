
export class binding_t {
    constructor(port) {
        this.port = port;
    }

    async write(msg) {
        const textEncoder = new TextEncoderStream();
        const writableStreamClosed = textEncoder.readable.pipeTo(this.port.writable);
        const writer = textEncoder.writable.getWriter();
        console.log('[SEND]', msg);
        writer.write(msg + '\n');
        writer.close();
        await writableStreamClosed;
        writer.releaseLock();
        console.log("Released writer lock.")
    }

    async read_output() {
        const textDecoder = new TextDecoderStream();
        const readableStreamClosed = this.port.readable.pipeTo(textDecoder.writable);
        const reader = textDecoder.readable.getReader();

        // Listen to data coming from the serial device.
        let start_time = Date.now();
        let msgs = "";
        try {
            while (Date.now() - start_time < 100000)
            {
                const { value, done } = await reader.read();
                msgs += value;
                if (msgs.includes('}============'))
                {
                    console.log("Finished reading OSM.");
                    break;
                }
            }
        }
        catch (error) {
            console.log(error);
        }
        finally {
            reader.cancel();
            await readableStreamClosed.catch(() => { /* Ignore the error */ });
            reader.releaseLock();
            console.log("Reader released.");
            return msgs;
        }
    }

    async parse_msg(msg) {
        let start, end;
        if (typeof (msg) !== 'string') {
            console.log(typeof (msg));
            return
        }
        let spl = msg.split("\n\r");
        spl.forEach((s, i) => {
            if (s === "============{")
            {
                start = i + 1;
            }
            else if (s === "}============")
            {
                end = i;
            }
            if (s.includes("DEBUG") || s.includes("ERROR"))
            {
                start += 1;
            }
        })
        if (spl)
        {
            let sliced = spl.slice(start, end)[0];
            let sl = sliced.split(": ");
            let r = sl[sl.length - 1];
            return r;
        }
        return 0;
    }

    async do_cmd(cmd)
    {
        await this.write(cmd);
        let output = await this.read_output();
        let parsed = await this.parse_msg(output);
        return parsed;
    }

    async get_measurements() {
        this.write("measurements");
        let meas = await this.read_output();
        let measurements = [];
        let meas_split = meas.split("\n\r");
        let start, end, regex, match, in_str, interval, interval_mins;
        meas_split.forEach((i, index) => {
            let m = i.split(/[\t]{1,2}/g);
            if (i === "Name	Interval	Sample Count") {
                start = index;
                m.push("Last Value");
                measurements.push(m);
            }
            else if (i === "}============") {
                end = index;
            }
            else {
                try {
                    in_str = m[1];
                    regex = /^(\d+)x(\d+)mins$/;
                    match = in_str.match(regex);
                    if (match) {
                        interval = match[1];
                        interval_mins = match[2];
                    }
                    m[1] = interval;
                    m.push("");
                }
                catch (error) {
                    console.log(error);
                }
                finally {
                    measurements.push(m);
                }
            }
        })
        let extracted_meas = measurements.slice(start, end);
        let imins = extracted_meas[0][1].concat(" " + "(" + interval_mins + "mins)");
        extracted_meas[0][1] = imins;
        return extracted_meas;
    }

    async get_lora_deveui() {
        let dev_eui = await this.do_cmd("comms_config dev-eui");
        return dev_eui;
    }

    async get_lora_appkey() {
        let appkey = await this.do_cmd("comms_config app-key")
        return appkey;
    }

    async get_lora_region() {
        let region = await this.do_cmd("comms_config region")
        return region;
    }

    async get_lora_conn() {
        let conn = await this.do_cmd("comms_conn")
        return conn;
    }

    async get_wifi_config() {
        /*
            comms_pr_cfg

            WIFI SSID: Wifi Example SSID
            WIFI PWD: wifipwd
            MQTT ADDR: mqtt_addr
            MQTT USER: mqttuser
            MQTT PWD: mqttpwd
            MQTT CA: server
            MQTT PORT: 8883
        */
    }

    async get_value(meas)
    {
        let cmd = "get_meas " + meas
        let res = await this.do_cmd(cmd);
        return res;

    }
}


