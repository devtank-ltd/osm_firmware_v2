export class navbar_t
{
  constructor(id)
  {
    this.id = id;
    this.navbar_html = `
    <div class="topnav" id="topnav">
        <a id="home">Home</a>
        <a id="console">Console</a>
        <a id="modbus">Modbus</a>
      <img id="nav-img" src="./imgs/osm.png" alt="OpenSmartMonitor Navbar Logo">
    </div>
    `;
  }

  insert_navbar()
  {
    document.getElementById('navbar').innerHTML = this.navbar_html;
  }

  change_active_tab()
  {
    this.insert_navbar();
    document.getElementById(this.id).classList.add("active");
  }

}

