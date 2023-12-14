const navbar = `
<div class="topnav" id="topnav">
    <a id="home">Home</a>
    <a id="console">Console</a>
    <a id="modbus">Modbus</a>
  <img id="nav-img" src="./imgs/osm.png" alt="OpenSmartMonitor Navbar Logo">
</div>
`


async function insert_navbar()
{
  document.getElementById('navbar').innerHTML = navbar;
}

export async function add_active(id_)
{
  await insert_navbar();
  document.getElementById(id_).classList.add("active");
}

