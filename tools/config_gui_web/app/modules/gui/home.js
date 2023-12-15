import { navbar_t } from './navbar.js'
import { measurements_table_t } from './measurements_table.js'
import { lora_config_t } from './lora_config.js';

export class home_tab_t {
    constructor(dev) {
        this.dev = dev;
    }

    async insert_homepage() {
        const homepage = `
        <body>
            <div id="navbar"></div>
            <button id='main-page-disconnect'>Disconnect</button>

            <div class="home-page-div">
                <div class="measurements-table"></div>
                <div class="lora-config-table"></div>
            </div>
            <script type="module" async src="./modules/gui/navbar.js"></script>
            <script type="module" async src="./modules/gui/measurements_table.js"></script>
            <script type="module" async src="./modules/gui/lora_config.js"></script>
        </body>
`
        let doc = document.getElementById('main-page-body');
        doc.innerHTML = homepage;

        let active_tab = new navbar_t("home");
        active_tab.change_active_tab();

        let meas_table = new measurements_table_t(this.dev);
        await meas_table.create_measurements_table_gui();

        let lora = new lora_config_t(this.dev);
        await lora.populate_lora_fields();
    }

}

