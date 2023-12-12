const title = "LoRaWAN Configuration"
const lora_headers = [ "Device EUI", "Application Key", "Connected" ]

const lora_res = document.querySelector('div.lora-config-table');
const lora_tbl = lora_res.appendChild(document.createElement('table'));
const lora_tBody = lora_tbl.createTBody();
lora_tbl.createTHead()


let lora_row = lora_tbl.tHead.insertRow();
let cell = lora_row.insertCell()
cell.colSpan = 2;
cell.textContent = title;

lora_headers.forEach((i) =>
{
    let r = lora_tBody.insertRow();
    r.insertCell().textContent = i;
    if (i === "Device EUI")
        r.insertCell().textContent = "sh4shk8yab5swkjf";
    else if (i === "Application Key")
        r.insertCell().textContent = "ksyutnab6jsk58akngt482jsnyisnt8";
    else if (i === "Connected")
        r.insertCell().textContent = "Yes";
})


