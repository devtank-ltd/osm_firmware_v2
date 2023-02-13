import sqlite3
import yaml
import os
import sys

# self.cur.execute("INSERT OR IGNORE INTO devices (unit_id, byte_order, name, baudrate, bits, parity, stop_bits, binary) VALUES (5, 'MSB MSW', 'E53', 9600, 8, 'N', 1, 'RTU')")
# self.cur.execute("INSERT OR IGNORE INTO devices (unit_id, byte_order, name, baudrate, bits, parity, stop_bits, binary) VALUES (1, 'MSB LSW', 'RIF', 9600, 8, 'N', 1, 'RTU')")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc56e', 3, 'U32', 'PF', 'Power Factor', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc552', 3, 'U32', 'cVP1', 'Voltage Phase 1', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc554', 3, 'U32', 'cVP2', 'Voltage Phase 2', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc556', 3, 'U32', 'cVP3', 'Voltage Phase 3', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc560', 3, 'U32', 'mAP1', 'Milliamps Phase 1', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc562', 3, 'U32', 'mAP2', 'Milliamps Phase 2', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc564', 3, 'U32', 'mAP3', 'Milliamps Phase 3', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0xc652', 3, 'U32', 'ImEn', 'Import Energy', 1)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x36', 4, 'F', 'APF', 'Power Factor', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x00', 4, 'F','VP1', 'Volts P1', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x02', 4, 'F', 'VP2', 'Volts P2', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x04', 4, 'F', 'VP3', 'Volts P3', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x10', 4, 'F', 'AP1', 'Amps P1', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x12', 4, 'F', 'AP2', 'Amps P2', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x14', 4, 'F', 'AP3', 'Amps P3', 2)")
# self.cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id) VALUES ('0x60', 4, 'F', 'Imp', 'Import Energy', 2)")
# self.cur.execute("INSERT OR IGNORE INTO templates (template_name, description) VALUES ('Countis E53', 'Countis E53 Modbus')")
# self.cur.execute("INSERT OR IGNORE INTO templates (template_name, description) VALUES ('Rayleigh RI F200', 'Rayleigh RI F Modbus')")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 1)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 2)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 3)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 4)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 5)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 6)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 7)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 8)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 9)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 10)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 11)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 12)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 13)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 14)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 15)")
# self.cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 16)")
# self.cur.commit()


DROP_TABLE = lambda table_n: "DROP TABLE %s" % table_n


CREATE_DEV_TABLE = '''CREATE TABLE IF NOT EXISTS devices
                            (
                                id INTEGER PRIMARY KEY AUTOINCREMENT,
                                unit_id INTEGER NOT NULL,
                                byte_order VARCHAR(255) NOT NULL,
                                name VARCHAR(255) NOT NULL,
                                baudrate INTEGER NOT NULL,
                                bits INTEGER NOT NULL,
                                parity VARCHAR(255) NOT NULL,
                                stop_bits INTEGER NOT NULL,
                                binary VARCHAR(255) NOT NULL
                                
                            )'''

CREATE_REG_TABLE = '''CREATE TABLE IF NOT EXISTS registers 
                            (
                                id INTEGER PRIMARY KEY AUTOINCREMENT,
                                hex_address VARCHAR(255) NOT NULL,
                                function_id INTEGER NOT NULL,
                                data_type VARCHAR(255) NOT NULL,
                                reg_name VARCHAR(255) NOT NULL,
                                reg_desc VARCHAR(255) NOT NULL,
                                device_id INTEGER NOT NULL,
                                template_id INTEGER NOT NULL,
                                FOREIGN KEY (device_id) REFERENCES devices (id),
                                FOREIGN KEY (template_id) REFERENCES templates (id)
                            )'''

CREATE_TEMP_TABLE = '''CREATE TABLE IF NOT EXISTS templates
                            (
                                id INTEGER PRIMARY KEY AUTOINCREMENT,
                                name VARCHAR(255) NOT NULL UNIQUE,
                                description VARCHAR(255) NOT NULL
                            )'''

CREATE_TEMP_REG_TABLE = ''' CREATE TABLE IF NOT EXISTS templateRegister
                            (
                                id INTEGER PRIMARY KEY,
                                template_id INTEGER,
                                register_id INTEGER,
                                FOREIGN KEY (template_id) REFERENCES templates (template_id),
                                FOREIGN KEY (register_id) REFERENCES registers (register_id)
                            )'''

CREATE_MEASUREMENT_TABLE = ''' CREATE TABLE IF NOT EXISTS measurements
                            (
                                id INTEGER PRIMARY KEY,
                                handle VARCHAR(4) NOT NULL UNIQUE,
                                description VARCHAR(255),
                                reference FLOAT NOT NULL,
                                threshold FLOAT NOT NULL
                            )'''


INS_TMP_REG = lambda tmp_id, reg_id : '''INSERT INTO templateRegister (template_id, register_id)
                                            VALUES (%u, %u)''' % (tmp_id, reg_id)


INS_INTO_REGS = lambda hex, func, data_t, reg_n, reg_d, dev_id, tmp_id : '''
    INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id)
    VALUES ('%s', %u, '%s', '%s', '%s', %u, %u)''' % (hex, func, data_t, reg_n, reg_d, dev_id, tmp_id)

# UPDATE_REGS = lambda hex, func, data_t, reg_n, reg_d, dev_id : f'''
#     UPDATE registers
#     SET hex_address = '{hex}', function_id = {func}, data_type = '{data_t}',
#     reg_name = '{reg_n}', reg_desc = '{reg_d}', device_id = {dev_id}
#     WHERE registers.reg_name = '{reg_n}' AND registers.device_id = {dev_id}
#     '''
UPDATE_REGS = lambda hex, func, data_t, reg_n, reg_d, dev_id, tmp_id : '''
    REPLACE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id)
    VALUES ('%s', %u, '%s', '%s', '%s', %u, %u)''' % (hex, func, data_t, reg_n, reg_d, dev_id, tmp_id)

GET_DEV_ID = lambda dev: "SELECT id FROM devices WHERE name = '%s'" % dev

GET_DEV_NAME = lambda dev_uid: "SELECT name FROM devices WHERE unit_id = %u" % dev_uid


INS_INTO_DEV = lambda uid, bytes, dev, baud, bits, pari, stop_b, bin: '''INSERT OR IGNORE INTO
            devices (unit_id, byte_order, name, baudrate, bits, parity, stop_bits, binary)
            VALUES ('%s','%s','%s','%s','%s','%s','%s','%s')''' % (uid, bytes, dev, baud, bits, pari, stop_b, bin)

# UPDATE_DEV = lambda uid, bytes, dev, baud, bits, pari, stop_b, bin: f'''UPDATE devices
#             SET unit_id = '{uid}', byte_order = '{bytes}', name = '{dev}', baudrate = '{baud}',
#             bits = '{bits}', parity = '{pari}', stop_bits = '{stop_b}', binary = '{bin}'
#             WHERE devices.name = '{dev}'
#             '''

UPDATE_DEV = lambda uid, bytes, dev, baud, bits, pari, stop_b, bin: '''REPLACE INTO
            devices (unit_id, byte_order, name, baudrate, bits, parity, stop_bits, binary)
            VALUES ('%s','%s','%s','%s','%s','%s','%s','%s')''' % (uid, bytes, dev, baud, bits, pari, stop_b, bin)


MAX_TMP_ID = "SELECT id FROM templates WHERE id = (SELECT MAX(id) FROM templates)"


INS_TEMP = lambda temp_name, desc: '''INSERT OR IGNORE INTO templates (name, description)
                                VALUES ('%s', '%s')''' % (temp_name, desc)


DEL_TEMP_ID = lambda temp: "DELETE FROM templates WHERE id = %u" % temp

DEL_REGS = lambda tmp_id: "DELETE FROM registers WHERE template_id = %u" % tmp_id

DEL_DEV = lambda dev: "DELETE FROM devices WHERE name='%s'" % dev

DEL_TEMP_REG = lambda temp_id: "DELETE FROM templateRegister WHERE template_id = %u" % temp_id


GET_TEMP_ID = lambda temp: "SELECT id FROM templates WHERE name = '%s'" % temp


GET_DEVS_REGS = lambda temp_id: '''SELECT devices.unit_id,
                                    devices.byte_order,
                                    registers.hex_address,
                                    registers.function_id,
                                    registers.data_type,
                                    registers.reg_name,
                                    devices.name
                                    FROM registers, devices
                                    WHERE registers.template_id=%u
                                    AND registers.device_id = devices.id''' % temp_id


GET_ALL_INFO_DEVS_REGS = lambda temp_n: ''' 
                            SELECT
                            devices.unit_id,
                            devices.byte_order,
                            devices.name,
                            devices.baudrate,
                            devices.bits,
                            devices.parity,
                            devices.stop_bits,
                            devices.binary,
                            registers.hex_address,
                            registers.function_id,
                            registers.data_type, 
                            registers.reg_name,
                            registers.reg_desc,
                            templates.description
                            FROM registers, templates, devices
                            WHERE registers.template_id = templates.id
                            AND registers.device_id = devices.id
                            AND templates.name = '%s'
                            ''' % temp_n

GET_ALL_REG = lambda reg_n: '''SELECT hex_address, function_id, data_type,  reg_name,
                              reg_desc, device_id FROM registers WHERE reg_name="%s"''' % reg_n


GET_UNIT_IDS = "SELECT unit_id, name from devices"

GET_HEX_REG = lambda hex, reg: '''SELECT id FROM registers 
                            WHERE hex_address = '%s'
                            AND registers.reg_name = '%s'
                          ''' % (hex, reg)

GET_TMP_N = "SELECT name FROM templates"

GET_REG_DESC = lambda reg: "SELECT reg_desc FROM registers WHERE reg_name = '%s'" % reg

GET_MEASUREMENT_INFO = "SELECT description, reference, threshold FROM measurements WHERE handle = ?"

GET_MODBUS_MEASUREMENT_INFO = "SELECT reg_desc, reference, threshold FROM registers WHERE reg_name = ?"


def find_path():
    PATH = os.environ.get("_MEIPASS2",os.path.abspath("."))
    return PATH

class modb_database_t(object):
    def __init__(self, path):
        self.path = path
        self.conn = sqlite3.connect(path + '/config_database/modbus_templates_two.db', check_same_thread=False)
        self.cur = self.conn.cursor()

    def get_reg_ids(self, hex_addr, reg_name):
        reg_ids = []
        if type(hex_addr) == list:
            for i in hex_addr:
                reg = self.cur.execute(GET_HEX_REG(i, reg_name))
                reg_id = reg.fetchall()[0]
                reg_ids.append(reg_id)
        else:
            reg = self.cur.execute(GET_HEX_REG(hex_addr, reg_name))
            reg_ids = reg.fetchall()[0]
        return reg_ids

    def get_modbus_template_regs(self, chosen_template):
        temp_id = None
        self.results = []
        with self.conn as conn:
            correct_temp = conn.execute(GET_TEMP_ID(chosen_template))
            for row in correct_temp:
                temp_id = row[0]
            if temp_id:
                data = conn.execute(GET_DEVS_REGS(temp_id))
                for row in data:
                    self.results.append(row)
        return self.results

    def get_all_info(self, template):
        devices_dict = [
            {
                'devices': [
                    {
                        'unit_id':   '',
                        'byte_order':  '',
                        'name':  '',
                        'baudrate':  '',
                        'bits':  '',
                        'parity':  '',
                        'stop_bits':  '',
                        'binary':  ''
                    }
                ],
                'registers': [
                    {
                        'hex_address':  [],
                        'function_id':  [],
                        'data_type':  [],
                        'reg_name':  [],
                        'reg_desc':  []
                    }
                ],
                'templates': [
                    {
                        'name': '',
                        'description': ''
                    }
                ]
            }
        ]
        with self.conn as conn:
            # correct_temp = conn.execute(GET_TEMP_ID(template))
            # temp_id = correct_temp.fetchone()[0]
            data = conn.execute(GET_ALL_INFO_DEVS_REGS(template))
            for row in data:
                register = devices_dict[0]['registers'][0]
                register['hex_address'].append(row[8])
                register['function_id'].append(row[9])
                register['data_type'].append(row[10])
                register['reg_name'].append(row[11])
                register['reg_desc'].append(row[12])
            device = devices_dict[0]['devices'][0]
            device['unit_id'] = row[0]
            device['byte_order'] = row[1]
            device['name'] = row[2]
            device['baudrate'] = row[3]
            device['bits'] = row[4]
            device['parity'] = row[5]
            device['stop_bits'] = row[6]
            device['binary'] = row[7]
            tmpl = devices_dict[0]['templates'][0]
            tmpl['name'] = template
            tmpl['description'] = row[13]
        return devices_dict

    def get_regs(self, chosen_template):
        results = []
        with open(self.path + '/yaml_files/modbus_data.yaml', 'r') as f:
            document = yaml.full_load(f)
            for doc in document:
                register = doc['registers'][0]
                hex_addr_list = register['hex_address']
                function_id_list = register['function_id']
                data_type_list = register['data_type']
                reg_name_list = register['reg_name']
                reg_desc_list = register['reg_desc']
                temp_name = doc['templates'][0]['name']
                if chosen_template == temp_name:
                    for v in range(len(hex_addr_list)):
                        a_list = []
                        a_list.append(hex_addr_list[v])
                        a_list.append(function_id_list[v])
                        a_list.append(data_type_list[v])
                        a_list.append(reg_name_list[v])
                        a_list.append(reg_desc_list[v])
                        results.append(a_list)
        return results

    def get_full_regs(self, reg_name):
        results = []
        data = self.conn.execute(GET_ALL_REG(reg_name))
        for row in data:
            results.append(row)
        return results

    def get_measurement_info(self, handle):
        with self.conn as conn:
            data = conn.execute(GET_MEASUREMENT_INFO, (str(handle),))
        return data.fetchone()

    def get_modbus_measurement_info(self, handle):
        with self.conn as conn:
            data = conn.execute(GET_MODBUS_MEASUREMENT_INFO, (str(handle),))
        return data.fetchone()

if __name__ == "__main__":
    conn = sqlite3.connect('./config_database/modbus_templates_two.db')
    cur =  conn.cursor()
    cur.execute(CREATE_DEV_TABLE)
    cur.execute(CREATE_REG_TABLE)
    cur.execute(CREATE_TEMP_TABLE)
    # cur.execute(CREATE_TEMP_REG_TABLE)
    cur.execute(CREATE_MEASUREMENT_TABLE)
    cur.execute("INSERT OR IGNORE INTO templates (name, description) VALUES ('Countis E53', 'Countis E53 Modbus')")
    cur.execute("INSERT OR IGNORE INTO templates (name, description) VALUES ('Rayleigh RI F200', 'Rayleigh RI F Modbus')")
    cur.execute("INSERT OR IGNORE INTO devices (unit_id, byte_order, name, baudrate, bits, parity, stop_bits, binary) VALUES (5, 'MSB MSW', 'E53', 9600, 8, 'N', 1, 'RTU')")
    cur.execute("INSERT OR IGNORE INTO devices (unit_id, byte_order, name, baudrate, bits, parity, stop_bits, binary) VALUES (1, 'MSB LSW', 'RIF', 9600, 8, 'N', 1, 'RTU')")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc56e', 3, 'U32', 'PF', 'Power Factor', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc552', 3, 'U32', 'cVP1', 'Voltage Phase 1', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc554', 3, 'U32', 'cVP2', 'Voltage Phase 2', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc556', 3, 'U32', 'cVP3', 'Voltage Phase 3', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc560', 3, 'U32', 'mAP1', 'Milliamps Phase 1', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc562', 3, 'U32', 'mAP2', 'Milliamps Phase 2', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc564', 3, 'U32', 'mAP3', 'Milliamps Phase 3', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0xc652', 3, 'U32', 'ImEn', 'Import Energy', 1, 1)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x36', 4, 'F', 'APF', 'Power Factor', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x00', 4, 'F','VP1', 'Volts P1', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x02', 4, 'F', 'VP2', 'Volts P2', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x04', 4, 'F', 'VP3', 'Volts P3', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x10', 4, 'F', 'AP1', 'Amps P1', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x12', 4, 'F', 'AP2', 'Amps P2', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x14', 4, 'F', 'AP3', 'Amps P3', 2, 2)")
    cur.execute("INSERT OR IGNORE INTO registers (hex_address, function_id, data_type, reg_name, reg_desc, device_id, template_id) VALUES ('0x60', 4, 'F', 'Imp', 'Import Energy', 2, 2)")

    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 1)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 2)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 3)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 4)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 5)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 6)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 7)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (1, 8)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 9)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 10)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 11)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 12)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 13)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 14)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 15)")
    # cur.execute("INSERT OR IGNORE INTO templateRegister (template_id, register_id) VALUES (2, 16)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('TEMP', 'Temperature', 20.0, 2.5)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('HUMI', 'Humidity', 50.0, 2.5)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('LGHT', 'Light', 1000.0, 100.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('TMP2', 'One Wire Probe', 25.0625, 0.01)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('PM25', 'PM 2.5', 20.0, 0.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('PM10', 'PM 10', 30.0, 0.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('CC1', 'Current Clamp Phase 1', 5000.0, 100.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('CC2', 'Current Clamp Phase 2', 7500.0, 100.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('CC3', 'Current Clamp Phase 3', 10000.0, 100.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('SND', 'Sound', 10.0, 5.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('FW', 'Firmware', 177522043.0, 0.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('BAT', 'Battery', 100.0, 0.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('FTA1', '4-20 mA', 4.0, 0.2)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('FTA2', '4-20 mA', 8.0, 0.2)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('FTA3', '4-20 mA', 16.0, 2.0)")
    cur.execute("INSERT OR IGNORE INTO measurements (handle, description, reference, threshold) VALUES ('FTA4', '4-20 mA', 20.0, 0.2)")

    conn.commit()
