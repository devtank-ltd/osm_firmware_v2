export class navbar_t {
  constructor(id) {
    this.id = id;
  }

  async insert_navbar() {
    this.resp = await fetch('modules/gui/html/navbar.html');
    this.text = await this.resp.text();
    document.getElementById('navbar').innerHTML = this.text;
  }

  async change_active_tab() {
    await this.insert_navbar();
    document.getElementById(this.id).classList.add('active');
  }
}
