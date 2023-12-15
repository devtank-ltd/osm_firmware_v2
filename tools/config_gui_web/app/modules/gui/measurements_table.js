export class measurements_table_t
{
    constructor(dev)
    {
        this.dev = dev;
        this.update_measurement_config = this.update_measurement_config.bind(this);
    }

    async create_measurements_table_gui() {

        let measurements = await this.dev.get_measurements();
        const res = document.querySelector('div.measurements-table');
        const tbl = res.appendChild(document.createElement('table'));
        const tBody = tbl.createTBody();
        tbl.createTHead()


        measurements.forEach((cell, i) => {
            if (i === 0) {
                /* Insert headers */
                let row = tbl.tHead.insertRow();
                cell.forEach((c) => {
                    row.insertCell().textContent = c;
                })
            }
            else {
                let r = tBody.insertRow();
                cell.forEach((j, index) => {
                    let entry = r.insertCell();
                    entry.textContent = j;
                    if (index === 1 || index === 2)
                    {
                        /* If entry is interval or sample count, user can edit the cell */
                        entry.contentEditable = true;
                        entry.addEventListener('focusout', this.update_measurement_config);
                    }
                })
            }
        })
    }

    async update_measurement_config(e) {
        let cell_index = e.srcElement.cellIndex;
        let row_index = e.target.parentNode.rowIndex;
        let table = e.target.offsetParent;

        let meas = table.rows[row_index].cells[0].innerHTML;
        let new_val = table.rows[row_index].cells[cell_index].innerHTML;
        let type = table.rows[0].cells[cell_index].innerHTML;

        /* Example: interval CNT1 1 */
        if (type.includes("Interval")) {
            this.dev.send_cmd("interval " + meas + " " + new_val);
        }
        else {
            this.dev.send_cmd("samplecount " + meas + " " + new_val);
        }
    }
}

