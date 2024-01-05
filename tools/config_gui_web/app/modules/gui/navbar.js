export class navbar_t {
  async insert_navbar() {
    this.resp = await fetch('modules/gui/html/navbar.html');
    this.text = await this.resp.text();
    document.getElementById('navbar').innerHTML = this.text;
  }

  async change_active_tab(id) {
    await this.insert_navbar();
    document.getElementById(id).classList.add('active');
  }
}
