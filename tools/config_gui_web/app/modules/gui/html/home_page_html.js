export const home_page = `<fieldset id="home-page-fieldset" class="home-page-fieldset">
<body>
    <div id="navbar"></div>

    <div class="home-page-div">
        <div class="measurements-table"></div>

        <div class="home-edit-uplink-field" id="home-change-uplink">
            <h3>Change Uplink Time</h3>
            <div>
                <input autocomplete="off"
                    type="number"
                    min="0"
                    id="home-uplink-input"
                    placeholder="Enter a number of minutes">
                </input>
                <input id="home-uplink-submit"type="submit">
            </div>
        </div>

        <div class="lora-config-table">
            <div class="buttons">
                <button id="lora-gen-dev-eui">Generate LoRa Dev EUI</button>
                <button id="lora-gen-app-key">Generate LoRa App Key</button>
                <button id="lora-send-config">Send</button>
            </div>
        </div>

        <div class="wifi-config-table"></div>

        <div class="home-load-save-config-buttons">
            <button  id="home-load-osm-config">Load Configuration</button>
            <button  id="home-save-osm-config">Save Configuration</button>
            <button  id="home-disconnect">Disconnect</button>
        </div>

        <div class="home-serial-firmware-config-div">
            <p id="home-serial-num">Failed to display serial number</p>
            <p id="home-firmware-version">Failed to display firmware version</p>
        </div>


    </div>
    <script type="module" async src="./modules/gui/navbar.js"></script>
    <script type="module" async src="./modules/gui/measurements_table.js"></script>
    <script type="module" async src="./modules/gui/lora_config.js"></script>
</body>
</fieldset>
`;
