const navbar = `
<div class="topnav" id="topnav">
    <a id="home" href="home.html">Home</a>
    <a id="console" href="console.html">Console</a>
    <a id="modbus" href="modbus.html">Modbus</a>
  <img id="nav-img" src="../imgs/osm.png" alt="OpenSmartMonitor Navbar Logo">
</div>
`


function insert_navbar()
{
  document.getElementById('navbar').innerHTML = navbar;
}

function add_active(id_)
{
  insert_navbar();
  document.getElementById(id_).classList.add("active");
}

