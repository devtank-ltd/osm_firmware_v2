import { navbar } from './html/navbar_html.js';

export class navbar_t {
  constructor(id) {
    this.id = id;
    this.navbar_html = navbar;
  }

  insert_navbar() {
    document.getElementById('navbar').innerHTML = this.navbar_html;
  }

  change_active_tab() {
    this.insert_navbar();
    document.getElementById(this.id).classList.add('active');
  }
}
