const headers = [ "Measurement", "Uplink(15m)", "Sample Count", "Last Value" ]
const measurements = [
                    ["TEMP", 1, 5,  23 ],
                    ["HUMI", 1, 5,  53 ],
                    ["LGHT", 1, 5,  238],
                    ["SND",  1, 5,  65 ],
                    ["PM10", 1, 5,  2.3],
                    ["PM25", 1, 5,  5  ],
                    ["CC1",  1, 25, 7  ],
                    ["CC2",  1, 25, 9  ],
                    ["CC3",  1, 25, 12 ],
                    ["FTA1", 1, 2,  233],
                    ["FTA2", 1, 2,  432],
                    ["FTA3", 1, 2,  663],
                    ["FTA4", 1, 2,  823],
]

const res = document.querySelector('div.measurements-table');
const tbl = res.appendChild(document.createElement('table'));
const tBody = tbl.createTBody();
tbl.createTHead()

let row = tbl.tHead.insertRow();
headers.forEach((i) =>
{
    row.insertCell().textContent = i;
})

measurements.forEach((cell, i) =>
{
    let r = tBody.insertRow();
    cell.forEach((j) =>
    {
        r.insertCell().textContent = j;
    })
})
