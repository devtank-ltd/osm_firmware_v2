import { insert_homepage } from "../gui/home.js";
import { insert_config_gui_page } from "../gui/config_gui.js";

async function listen_open_serial()
{
    document.getElementById("main-page-connect").addEventListener("click", open_serial);
}

listen_open_serial();

let port;

async function open_serial()
{
    const filter = { usbVendorId: "0x10c4" }
    port = await navigator.serial.requestPort({ filters: [filter] });
    const { usbProductId, usbVendorId } = port.getInfo();

    await port.open({ baudRate: 115200, databits: 8, stopbits: 1, parity: 'none' });
    console.log("User connected to device ", port);

    navigator.serial.addEventListener("connect", (event) => {
        console.log("USB device available.")
      });

    navigator.serial.addEventListener("disconnect", (event) => {
        console.log("USB device disconnect detected.")
      });

    insert_homepage();
    document.getElementById("main-page-disconnect").addEventListener("click", close_serial);
}

async function close_serial()
{
    await port.close();
    port = null;
    console.log("Disconnected");
    insert_config_gui_page();
    listen_open_serial();
}

async function send_cmd(msg)
{
    const textEncoder = new TextEncoderStream();
    const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
    const writer = textEncoder.writable.getWriter();
    console.log('[SEND]', msg);
    writer.write(msg + '\n');
    writer.close();
    await writableStreamClosed;
    writer.releaseLock();
    console.log("Released writer lock.")
}

async function read_output()
{
    const textDecoder = new TextDecoderStream();
    const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
    const reader = textDecoder.readable.getReader();

    // Listen to data coming from the serial device.
    let start_time = Date.now();
    let msgs = [];

    try
    {
        while (Date.now() - start_time < 100000)
        {
            const { value, done } = await reader.read();
            if (value.includes("}============"))
            {
                msgs.push(value);
                console.log("DONE");
                break;
            }
            msgs.push(value);
        }
    }
    catch (error)
    {
        console.log(error);
    }
    finally
    {
        const result = msgs.join('').replace(/\\rn/g, '\rn')
        reader.cancel();
        await readableStreamClosed.catch(() => { /* Ignore the error */ });

        reader.releaseLock();
        console.log("Reader released.");
        return result;
    }
}

export async function get_measurements()
{
    send_cmd("measurements");
    let meas = read_output();
    let measurements = [];
    let meas_split = (await meas).split("\n\r");
    let start, end, regex, match, in_str, interval, interval_mins;
    meas_split.forEach((i, index) =>
    {
        let m = i.split(/[\t]{1,2}/g);
        if (i === "Name	Interval	Sample Count")
        {
            start = index;
            m.push("Last Value");
            measurements.push(m);
        }
        else if (i === "}============")
        {
            end = index;
        }
        else
        {
            try
            {
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
            catch (error)
            {
                console.log(error);
            }
            finally
            {
                measurements.push(m);
            }
        }
    })
    let extracted_meas = measurements.slice(start, end);
    let imins = extracted_meas[0][1].concat(" " + "(" + interval_mins + "mins)");
    extracted_meas[0][1] = imins;
    return extracted_meas;
}




