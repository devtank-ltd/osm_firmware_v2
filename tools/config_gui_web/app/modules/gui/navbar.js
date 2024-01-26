export class navbar_t {
  async insert_navbar() {
    this.resp = await fetch('modules/gui/html/navbar.html');
    this.text = await this.resp.text();
    document.getElementById('navbar').innerHTML = this.text;
  }

  async change_active_tab(id) {
    document.getElementById('home-tab').classList.remove('active');
    document.getElementById('console-tab').classList.remove('active');
    document.getElementById('modbus-tab').classList.remove('active');
    document.getElementById('current-clamp-tab').classList.remove('active');
    this.active = document.getElementById(id).classList.add('active');
  }
}
