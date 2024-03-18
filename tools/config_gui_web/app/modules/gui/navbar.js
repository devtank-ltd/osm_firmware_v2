import { disable_interaction } from './disable.js';

export class navbar_t {
    async insert_navbar() {
        this.resp = await fetch('modules/gui/html/navbar.html');
        this.text = await this.resp.text();
        document.getElementById('navbar').innerHTML = this.text;
    }

    async change_active_tab(id) {
        await disable_interaction(true);
        document.getElementById('home-tab').classList.remove('active');
        document.getElementById('console-tab').classList.remove('active');
        document.getElementById('adv-conf-tab').classList.remove('active');
        this.active = document.getElementById(id).classList.add('active');
        await disable_interaction(false);
    }
}
