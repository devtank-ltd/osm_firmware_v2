async function open_serial()
{
    const filter = { usbVendorId: "0x10c4" }
    port = await navigator.serial.requestPort({ filters: [filter] });
    const { usbProductId, usbVendorId } = port.getInfo();

    await port.open({ baudRate: 115200, databits: 8, stopbits: 1, parity: 'none' });
    console.log("Connected");

    navigator.serial.addEventListener("connect", (event) => {
        // TODO: Automatically open event.target or warn user a port is available.
        console.log("USB device available.")
      });

      navigator.serial.addEventListener("disconnect", (event) => {
        // TODO: Remove |event.target| from the UI.
        // If the serial port was opened, a stream error would be observed as well.
        console.log("USB device disconnect detected.")
      });

    document.getElementById('main-page-disconnect').disabled = false;
    document.getElementById('main-page-connect').disabled = true;

    send_cmd("?");
}

async function close_serial()
{
    await port.close();
    port = null;
    document.getElementById('main-page-connect').disabled = false;
    document.getElementById('main-page-disconnect').disabled = true;
    console.log("Disconnected");
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
    read_output();
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
            console.log(typeof(value));
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
        console.log(result);
        reader.cancel();
        await readableStreamClosed.catch(() => { /* Ignore the error */ });

        reader.releaseLock();
        console.log("Reader released.");
    }
}
