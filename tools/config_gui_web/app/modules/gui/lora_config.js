import { generate_random } from "../backend/binding.js";

export class lora_config_t{
    constructor(dev)
    {
        this.dev = dev;
        this.write_config = this.write_config.bind(this)
        this.region_dropdown = `
            <select class="lora-config-region-dropdown" id="lora-config-region-dropdown">
                <option>EU433 (0)</option>
                <option>CN470 (1)</option>
                <option>RU864 (2)</option>
                <option>IN865 (3)</option>
                <option>EU868 (4)</option>
                <option>US915 (5)</option>
                <option>AU915 (6)</option>
                <option>KR920 (7)</option>
                <option>AS923-1 (8)</option>
                <option>AS923-2 (9)</option>
                <option>AS923-3 (10)</option>
                <option>AS923-4 (11)</option>
            </select>
        `
    }


    async add_listeners()
    {
        let deveui = document.getElementById("lora-gen-dev-eui");
        deveui.onclick = this.random_deveui;

        let appbtn = document.getElementById("lora-gen-app-key");
        appbtn.onclick = this.random_appkey;

        let sendbtn = document.getElementById("lora-send-config");
        sendbtn.onclick = this.write_config;
    }

    async populate_lora_fields()
    {
        const title = "LoRaWAN Configuration"
        const lora_headers = ["Device EUI", "Application Key", "Region", "Status"]

        let dev_eui = await this.dev.get_lora_deveui();
        let app_key = await this.dev.get_lora_appkey();
        let region = await this.dev.get_lora_region();
        let conn = await this.dev.get_lora_conn();

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
            {
                let td = r.insertCell();
                td.textContent = dev_eui;
                td.id = "lora-dev-eui-value";
            }
            else if (i === "Application Key")
            {
                let td = r.insertCell();
                td.textContent = app_key;
                td.id = "lora-app-key-value";
            }
            else if (i === "Region")
            {
                let td = r.insertCell();
                td.innerHTML = this.region_dropdown;
                let sel = document.getElementById("lora-config-region-dropdown")
                for (let v = 0; v < sel.length; v++)
                {
                    if (region === sel[v].innerHTML)
                    {
                        console.log(region);
                        sel.selectedIndex = v;
                    }
                }
            }
            else if (i === "Status")
            {
                let td = r.insertCell();
                td.textContent = conn;
                td.id = "lora-status-value";
            }

        })
    }

    async random_appkey()
    {
        let appfield = document.getElementById("lora-app-key-value");
        let appkey = await generate_random(32);
        appfield.textContent = appkey;
    }

    async random_deveui()
    {
        let devfield = document.getElementById("lora-dev-eui-value");
        let deveui = await generate_random(16);
        devfield.textContent = deveui;
    }

    async write_config()
    {
        let dev = document.getElementById("lora-dev-eui-value").innerHTML;
        let app = document.getElementById("lora-app-key-value").innerHTML;
        let reg = document.getElementById("lora-config-region-dropdown").value.split(" ")[0];
        await this.dev.set_deveui(dev);
        await this.dev.set_appkey(app);
        await this.dev.set_region(reg);
    }
}


