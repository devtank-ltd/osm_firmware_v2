import yaml
import modbus_db
from modbus_db import modb_database_t, find_path
from tkinter import messagebox

GET_TEMP_ID = modbus_db.GET_TEMP_ID
DEL_TEMP_ID = modbus_db.DEL_TEMP_ID
DEL_REGS = modbus_db.DEL_REGS
DEL_DEV = modbus_db.DEL_DEV
INS_TEMP = modbus_db.INS_TEMP
MAX_TMP_ID = modbus_db.MAX_TMP_ID
INS_INTO_DEV = modbus_db.INS_INTO_DEV
GET_DEV_ID = modbus_db.GET_DEV_ID
INS_INTO_REGS = modbus_db.INS_INTO_REGS
GET_UNIT_IDS = modbus_db.GET_UNIT_IDS
GET_TMP_N = modbus_db.GET_TMP_N
UPDATE_DEV = modbus_db.UPDATE_DEV
UPDATE_REGS = modbus_db.UPDATE_REGS
GET_DEV_N = modbus_db.GET_DEV_NAME
GET_ALL_DEVS = modbus_db.GET_ALL_DEVS
GET_ALL_DEV_IDS = modbus_db.GET_ALL_DEV_IDS
GET_DEV_ID_REGS = modbus_db.GET_DEV_ID_REGS
DEL_DEV_ID = modbus_db.DEL_DEV_ID

PATH = find_path()

class modbus_funcs_t():
    def __init__(self, db):
        self.db = db

    def revert_mb(self):
        # clear the yaml files from previous sessions
        with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
            pass
        with open(PATH + '/yaml_files/del_file.yaml', 'w') as del_file:
            pass
        with self.db.conn:
            data = self.db.cur.execute(GET_TMP_N)
        return data

    def delete_temp_reg(self, temp, reg, copy):
        if copy == None:
            with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
                doc = yaml.full_load(f)
                for v, item in enumerate(doc):
                    if doc[v]['templates'][0]['name'] == temp:
                        for i, d in enumerate(doc[v]['registers'][0]['reg_name']):
                            if d == reg[3]:
                                register = doc[v]['registers'][0]
                                register['reg_name'].pop(i)
                                register['function_id'].pop(i)
                                register['data_type'].pop(i)
                                register['hex_address'].pop(i)
                                register['reg_desc'].pop(i)
                                with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as d_file:
                                    yaml.dump(doc, d_file)
                                    break

    def save_to_yaml(self, tmp_n, dsc_val, byts,
                     uid, regs, baud,
                     ch_bits, par, rtu,
                     dev_n, stp_b):
        devices_dict = [
            {
                'devices': [
                    {
                        'unit_id':          uid,
                        'byte_order':         byts,
                        'name':        dev_n,
                        'baudrate':         baud,
                        'bits':      ch_bits,
                        'parity':          par,
                        'stop_bits':         stp_b,
                        'binary':           rtu,
                    }
                ],
                'registers': [
                    {
                        'hex_address': [],
                        'function_id': [],
                        'data_type':   [],
                        'reg_name':    [],
                        'reg_desc': []
                    }
                ],
                'templates': [
                    {
                        'name':   tmp_n,
                        'description': dsc_val
                    }
                ]
            }
        ]
        for item in regs:
            register = devices_dict[0]['registers'][0]
            register['hex_address'].append(item[0])
            register['function_id'].append(item[1])
            register['data_type'].append(item[2])
            register['reg_name'].append(item[3])
            register['reg_desc'].append(item[4])
        with open(PATH + '/yaml_files/modbus_data.yaml', 'a') as f:
            yaml.dump(devices_dict, f)

        with open(PATH + '/yaml_files/modbus_data.yaml') as f:
            documents = yaml.full_load(f)
            for doc in documents:
                doc_reg = doc['registers'][0]
                hex_addr_list = doc_reg['hex_address']
                function_id_list = doc_reg['function_id']
                data_type_list = doc_reg['data_type']
                reg_name_list = doc_reg['reg_name']
                reg_desc_list = doc_reg['reg_desc']
                yaml_template_name = doc['templates'][0]['name']
            yaml_list = []
            for i in range(len(hex_addr_list)):
                new_list = []
                new_list.append(hex_addr_list[i])
                new_list.append(function_id_list[i])
                new_list.append(data_type_list[i])
                new_list.append(reg_name_list[i])
                new_list.append(reg_desc_list[i])
                yaml_list.append(new_list)
        return yaml_template_name

    def del_tmpl_db(self, chosen_del, devices_dict):
        with open(PATH + '/yaml_files/del_file.yaml', 'r') as del_file:
            doc = yaml.full_load(del_file)
            if doc is None:
                devices_dict[0]['templates_to_del']['template'].append(
                    chosen_del)
                with open(PATH + '/yaml_files/del_file.yaml', 'a') as del_file:
                    yaml.dump(devices_dict, del_file)
            else:
                with open(PATH + '/yaml_files/del_file.yaml', 'r') as del_file:
                    doc = yaml.full_load(del_file)
                    doc[0]['templates_to_del']['template'].append(
                        chosen_del)
                    with open(PATH + '/yaml_files/del_file.yaml', 'w') as del_file:
                        yaml.dump(doc, del_file)

    def replace_tmpl_yaml(self, chosen_template):
        with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
            doc = yaml.full_load(f)
            for y, item in enumerate(doc):
                if item['templates'][0]['name'] == chosen_template:
                    del doc[y]
                    if len(doc) == 0:
                        del doc
                    with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                        if f:
                            yaml.dump(doc, f)

    def save_edit_yaml(self, copy, edited_template, devices_dict, edited_temp):
        if copy == 'edit':
            with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
                doc = yaml.full_load(f)
                for i, v in enumerate(doc):
                    if doc[i]['templates'][0]['name'] == edited_template:
                        del doc[i]
                        if len(doc) == 0:
                            del doc
                        with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                            if f:
                                yaml.dump(doc, f)

    def del_redundant_devs(self):
        devs = self.db.cur.execute(GET_DEV_ID_REGS)
        devs_in_use = []
        devs_not_used = []
        for d in devs:
            if d[0] not in devs_in_use:
                devs_in_use.append(d[0])
        devices = self.db.cur.execute(GET_ALL_DEV_IDS)
        for i in devices:
            if i[0] not in devs_in_use:
                devs_not_used.append(i[0])
        if len(devs_not_used):
            for v in devs_not_used:
                self.db.cur.execute(DEL_DEV_ID(v))

    def save_template(self):
        with open(PATH + '/yaml_files/del_file.yaml', 'r') as del_file:
            document = yaml.full_load(del_file)
            if document:
                for doc in document[0]['templates_to_del']['template']:
                    chosen_template = doc
                    if doc:
                        self.db.cur.execute(DEL_TEMP_ID(chosen_template))
                        self.db.cur.execute(DEL_REGS(chosen_template))
        with open(PATH + '/yaml_files/modbus_data.yaml') as f:
            document = yaml.full_load(f)
            if document:
                for doc in document:
                    for i in doc['devices']:
                        register = doc['registers'][0]
                        device = doc['devices'][0]
                        tmpl = doc['templates'][0]
                        hex_addr_list = register['hex_address']
                        function_id_list = register['function_id']
                        data_type_list = register['data_type']
                        reg_name_list = register['reg_name']
                        reg_desc_list = register['reg_desc']
                        temp_name = tmpl['name']
                        exist_tmp = None
                        temp_exists = self.db.cur.execute(
                            GET_TEMP_ID(temp_name))
                        does_exist = temp_exists.fetchone()
                        if does_exist:
                            exist_tmp = does_exist[0]
                        reg_list = []
                        for i in range(len(hex_addr_list)):
                            new_list = []
                            new_list.append(hex_addr_list[i])
                            new_list.append(function_id_list[i])
                            new_list.append(data_type_list[i])
                            new_list.append(reg_name_list[i])
                            new_list.append(reg_desc_list[i])
                            reg_list.append(new_list)
                        yaml_unit_id = device['unit_id']
                        yaml_bytes = device['byte_order']
                        yaml_baudrate = device['baudrate']
                        yaml_bits = device['bits']
                        yaml_parity = device['parity']
                        yaml_stop_bits = device['stop_bits']
                        yaml_binary = device['binary']
                        yaml_dev_name = device['name']
                        yaml_template_name = tmpl['name']
                        yaml_description = tmpl['description']
                        dev_n = self.db.cur.execute(GET_DEV_N(int(yaml_unit_id)))
                        all_devs = self.db.cur.execute(GET_ALL_DEVS)
                        devs = []
                        for d in all_devs:
                            devs.append(d[0])
                        dev_n = dev_n.fetchone()
                        if exist_tmp == None:
                            if yaml_dev_name in devs:
                                messagebox.showerror("Error", "Duplicate of device name found.")
                                return False
                            self.db.cur.execute(
                                INS_TEMP(yaml_template_name, yaml_description))

                            if dev_n:
                                del_dev = dev_n[0]
                                self.db.cur.execute(DEL_DEV(del_dev))
                            self.db.cur.execute(INS_INTO_DEV(yaml_unit_id, yaml_bytes,
                                                            yaml_dev_name, yaml_baudrate,
                                                            yaml_bits, yaml_parity,
                                                            yaml_stop_bits, yaml_binary))
                            dev_id = self.db.cur.execute(GET_DEV_ID(yaml_dev_name))
                            dev_id_t = dev_id.fetchone()[0]
                            d = self.db.cur.execute(MAX_TMP_ID)
                            tmp_id = d.fetchone()[0]
                            for i in reg_list:
                                yaml_hex = i[0]
                                yaml_func = int(i[1])
                                yaml_data_t = i[2]
                                yaml_reg_name = i[3]
                                yaml_reg_desc = i[4]
                                self.db.cur.execute(INS_INTO_REGS(yaml_hex, yaml_func, yaml_data_t,
                                                                yaml_reg_name, yaml_reg_desc, dev_id_t, tmp_id))
                        else:
                            self.db.cur.execute(DEL_REGS(exist_tmp))
                            if dev_n:
                                del_dev = dev_n[0]
                                self.db.cur.execute(DEL_DEV(del_dev))
                            self.db.cur.execute(INS_INTO_DEV(yaml_unit_id, yaml_bytes,
                                                            yaml_dev_name, yaml_baudrate,
                                                            yaml_bits, yaml_parity,
                                                            yaml_stop_bits, yaml_binary))
                            dev_id = self.db.cur.execute(GET_DEV_ID(yaml_dev_name))
                            dev_id_t = dev_id.fetchone()[0]
                            self.db.cur.execute(DEL_TEMP_ID(exist_tmp))
                            self.db.cur.execute(
                                INS_TEMP(yaml_template_name, yaml_description))
                            d = self.db.cur.execute(GET_TEMP_ID(yaml_template_name))
                            tmp_id = d.fetchone()[0]
                            for i in reg_list:
                                yaml_hex = i[0]
                                yaml_func = int(i[1])
                                yaml_data_t = i[2]
                                yaml_reg_name = i[3]
                                yaml_reg_desc = i[4]
                                self.db.cur.execute(INS_INTO_REGS(yaml_hex, yaml_func, yaml_data_t,
                                                                yaml_reg_name, yaml_reg_desc, dev_id_t, tmp_id))
            self.del_redundant_devs()
            with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                pass
            self.db.conn.commit()
            return True
