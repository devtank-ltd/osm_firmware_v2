import { get_measurements } from '../backend/serial.js'

export async function create_measurements_table_gui() {

    let measurements = await get_measurements();
    const res = document.querySelector('div.measurements-table');
    const tbl = res.appendChild(document.createElement('table'));
    const tBody = tbl.createTBody();
    tbl.createTHead()


    measurements.forEach((cell, i) => {
        if (i === 0) {
            let row = tbl.tHead.insertRow();
            cell.forEach((c) => {
                row.insertCell().textContent = c;
            })
        }
        else {
            let r = tBody.insertRow();
            cell.forEach((j) => {
                r.insertCell().textContent = j;
            })
        }
    })
}
