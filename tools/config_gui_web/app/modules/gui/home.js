import { add_active } from './navbar.js'
import { create_measurements_table_gui } from './measurements_table.js'
import { populate_lora_fields } from './lora_config.js';

export async function insert_homepage()
{
    const homepage =`
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
    add_active("home");
    await create_measurements_table_gui();
    populate_lora_fields();

}
