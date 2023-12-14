export function insert_config_gui_page()
{
    const config_gui = `<h1 id="main-page-title">OpenSmartMonitor Configuration</h1>
                        <button id='main-page-connect'>Connect</button>`

    let body = document.getElementById('main-page-body');
    body.innerHTML = config_gui;


}
