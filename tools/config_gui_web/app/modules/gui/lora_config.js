import { get_lora_conn, get_lora_appkey, get_lora_deveui, get_lora_region } from "../backend/serial.js";

export async function populate_lora_fields() {
    const title = "LoRaWAN Configuration"
    const lora_headers = ["Device EUI", "Application Key", "Region", "Status"]

    let dev_eui = await get_lora_deveui();
    let app_key = await get_lora_appkey();
    let region = await get_lora_region();
    let conn = await get_lora_conn();

    const lora_res = document.querySelector('div.lora-config-table');
    const lora_tbl = lora_res.appendChild(document.createElement('table'));
    const lora_tBody = lora_tbl.createTBody();
    lora_tbl.createTHead()

    let lora_row = lora_tbl.tHead.insertRow();
    let cell = lora_row.insertCell()
    cell.colSpan = 2;
    cell.textContent = title;

    lora_headers.forEach((i) => {
        let r = lora_tBody.insertRow();
        r.insertCell().textContent = i;
        if (i === "Device EUI")
            r.insertCell().textContent = dev_eui;
        else if (i === "Application Key")
            r.insertCell().textContent = app_key;
        else if (i === "Region")
            r.insertCell().textContent = region;
        else if (i === "Status")
            r.insertCell().textContent = conn;
    })
}

