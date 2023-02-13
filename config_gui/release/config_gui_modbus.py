import tkinter
import config_gui
from config_gui import *

class modbus_funcs_t(config_gui.config_gui_window_t):
    def _main_modbus_w(self):
        name = Label(self._modb_fr, text="Modbus Device Templates",
                     font=FONT, bg=IVORY)
        name.grid(column=1, row=0)

        self.template_list = Listbox(
            self._modb_fr, height=10, exportselection=False)
        self.template_list.grid(column=1, ipadx=150,
                                ipady=50, row=1, rowspan=7, sticky=NSEW)
        self.template_list.bind("<<ListboxSelect>>", self._callback)

        delete_button = Button(self._modb_fr, text="Delete",
                               command=lambda: self._delete_template(
                                   self.template_list),
                                   bg=IVORY, fg=BLACK, font=FONT,
                               activebackground="green", activeforeground=IVORY)
        delete_button.grid(column=2, row=1, sticky=EW)
        tt_del_btn = Hovertip(delete_button, 'Remove template.')

        edit_template_button = Button(self._modb_fr, text="Edit",
                                      command=lambda: self._add_template_w(
                                          'edit'),
                                      bg=IVORY, fg=BLACK, font=FONT,
                                      activebackground="green", activeforeground=IVORY)
        edit_template_button.grid(column=2, row=2, sticky=EW)
        tt_edit_btn = Hovertip(edit_template_button,
                               'Edit template.')

        add_template_button = Button(self._modb_fr, text="Add",
                                     command=lambda: self._add_template_w(
                                         None),
                                     bg=IVORY, fg=BLACK, font=FONT,
                                     activebackground="green", activeforeground=IVORY)
        add_template_button.grid(column=2, row=3, sticky=EW)
        tt_add_btn = Hovertip(add_template_button, 'Create a new template.')

        apply_template_button = Button(self._modb_fr, text="Apply",
                                       command=lambda: self._write_to_dev(
                                           self.template_list),
                                       bg=IVORY, fg=BLACK, font=FONT,
                                       activebackground="green", activeforeground=IVORY)
        apply_template_button.grid(column=2, row=4, sticky=EW)
        tt_apply_btn = Hovertip(apply_template_button,
                                'Write template to device.')

        copy_template_button = Button(self._modb_fr, text="Copy",
                                      command=lambda: self._add_template_w(
                                          " (copy)"),
                                      bg=IVORY, fg=BLACK, font=FONT,
                                      activebackground="green", activeforeground=IVORY)
        copy_template_button.grid(column=2, row=5, sticky=EW)
        tt_copy_btn = Hovertip(copy_template_button,
                               'Duplicate template.')

        revert_template_button = Button(self._modb_fr, text="Revert",
                                        command=self._reset_listb,
                                        bg=IVORY, fg=BLACK, font=FONT,
                                        activebackground="green", activeforeground=IVORY)
        revert_template_button.grid(column=2, row=6, sticky=EW)
        tt_revert_btn = Hovertip(
            revert_template_button, 'Undo unsaved changes.')

        save_template_button = Button(self._modb_fr, text="Save",
                                      command=self._send_to_save,
                                      bg=IVORY, fg=BLACK, font=FONT,
                                      activebackground="green", activeforeground=IVORY)
        save_template_button.grid(column=2, row=7, sticky=EW)
        tt_save_btn = Hovertip(save_template_button, 'Save all changes.')

        unit_id = Label(self._modb_fr, text="Modbus Registers on Template",
                        font=FONT, bg=IVORY)
        unit_id.grid(column=1, row=8)

        self.template_regs_box = Listbox(self._modb_fr, exportselection=False)
        self.template_regs_box.grid(
            column=1, ipadx=150, ipady=50, row=9, rowspan=4, sticky=EW)

        del_reg_button = Button(self._modb_fr, text="Delete",
                                command=lambda:
                                self._del_reg_tmp(self.template_regs_box, None,
                                                  self.template_list,
                                                  None, 'temp_reg_box'),
                                bg=IVORY, fg=BLACK, font=FONT,
                                activebackground="green", activeforeground=IVORY)
        del_reg_button.grid(column=2, row=9, sticky=EW)
        tt_del_r_btn = Hovertip(del_reg_button, 'Remove register.')

        shift_up_button = Button(self._modb_fr, text="▲",
                                 command=lambda:
                                 self._shift_up(self.template_regs_box,
                                                self.template_list,
                                                None, 'temp_reg_box'),
                                 bg=IVORY, fg=BLACK, font=FONT,
                                 activebackground="green", activeforeground=IVORY)
        shift_up_button.grid(column=2, row=10, sticky=EW)
        tt_sh_u_btn = Hovertip(shift_up_button, 'Shift up')

        shift_down_button = Button(self._modb_fr, text="▼",
                                   command=lambda:
                                   self._shift_down(self.template_regs_box,
                                                    self.template_list,
                                                    None, 'temp_reg_box'),
                                   bg=IVORY, fg=BLACK, font=FONT,
                                   activebackground="green", activeforeground=IVORY)
        shift_down_button.grid(column=2, row=11, sticky=EW)
        tt_sh_d_btn = Hovertip(shift_down_button, 'Shift down')

        self._current_mb = Label(
            self._modb_fr, text="Current Modbus Settings on OSM", font=FONT, bg=IVORY)
        self._current_mb.grid(column=3, row=0, columnspan=3, sticky=NSEW)
        self.template_list.curselection()
        with self.db.conn:
            data = self.db.cur.execute(GET_TMP_N)
            for row in data:
                dev = ''.join(row)
                self.template_list.insert(0, dev)
                write_dev_to_yaml = self.db.get_all_info(dev)
                with open(PATH + '/yaml_files/modbus_data.yaml', 'a') as f:
                    yaml.dump(write_dev_to_yaml, f)
        
        param_canv = Canvas(self._modb_fr)
        param_canv.grid(column=4, row=9, rowspan=5)

        img = Image.open(PARAMS)
        param_logo = ImageTk.PhotoImage(img)
        self._modb_fr.img_list = []
        dev_lab = Label(param_canv, image=param_logo, bg=IVORY,
                height=235, width=762)
        dev_lab.grid(column=0,row=0,sticky=EW)
        param_canv.bind('<Configure>', 
        lambda e : self._resize_image(e, dev_lab, img, self._modb_fr))
        self._modb_fr.img_list.append(param_logo)

        canv = Canvas(self._modb_fr)
        canv.grid(column=0, row=15, columnspan=10)

        self.img_os = Image.open(OPEN_S)
        os_logo = ImageTk.PhotoImage(self.img_os)
        self.os_lab = Label(canv, image=os_logo, bg=IVORY,
                height=111, width=1065)
        self.os_lab.grid(column=0, row=0, sticky=EW)
        self._modb_fr.img_list.append(os_logo)
        self._load_modbus(False)
        canv.bind('<Configure>', lambda e: self._resize_image(
            e, self.os_lab, self.img_os, self._modb_fr))
        self._modb_fr.columnconfigure([1,3,4,9], weight=1)
        self._modb_fr.rowconfigure([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14], 
            weight=1)
        
    def _close_save(self, window):
        if self._changes == True:
            yesno = tkinter.messagebox.askyesnocancel("Quit?",
                                                      "Would you like to save changes?")
            if yesno:
                self._send_to_save()
                window.destroy()
            elif yesno == False:
                window.destroy()
        else:
            window.destroy()

    def _send_to_save(self):
        log_func("User attempting to save a new template")
        self._modb_funcs.save_template()
        self._reset_listb()
        self._changes = False
        return self._changes

    def _delete_template(self, temp_list):
        confirm = tkinter.messagebox.askyesnocancel(
            "Delete?", "Are you sure you wish to delete template?")
        selection = temp_list.curselection()
        index = selection[0]
        chosen_template = temp_list.get(index)
        chosen_del = None
        chosen_del_temp = self.db.cur.execute(GET_TEMP_ID(chosen_template))
        does_exist = chosen_del_temp.fetchone()
        if does_exist:
            chosen_del = does_exist[0]
        devices_dict = [{
            'templates_to_del': {'template': []}
        }]
        if confirm:
            log_func("User attempting to delete modbus template...")
            self._changes = True
            if chosen_del != None:
                self._modb_funcs.del_tmpl_db(chosen_del, devices_dict)
            temp_list.delete(index)
            self._modb_funcs.replace_tmpl_yaml(chosen_template)

    def _edit_template(self, copy):
        self.edited_selection = self.template_list.curselection()
        if self.edited_selection:
            index = self.edited_selection[0]
            edited_template = self.template_list.get(index)
            with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
                doc = yaml.full_load(f)
                if doc:
                    for i, item in enumerate(doc):
                        doc_temp_name = doc[i]['templates'][0]['template_name']
                        if doc_temp_name == edited_template:
                            templ = doc[i]['templates'][0]
                            template_name = templ['template_name']
                            description = templ['description']
                            if copy == " (copy)":
                                self.name_entry.insert(0, template_name + copy)
                            else:
                                self.name_entry.insert(0, template_name)
                            self.desc_entry.insert(0, description)
                            device = doc[i]['devices'][0]
                            register = doc[i]['registers'][0]
                            device_name = device['name']
                            unit_id = device['unit_id']
                            bytes = device['byte_order']
                            baudrate = device['baudrate']
                            bits = device['bits']
                            parity = device['parity']
                            binary = device['binary']
                            stop_bits = device['stop_bits']
                            hex_addr_list = register['hex_address']
                            function_id_list = register['function_id']
                            data_type_list = register['data_type']
                            reg_name_list = register['reg_name']
                            desc_list = register['reg_desc']
                            self.unit_spinbox.delete(0, END)
                            self.unit_spinbox.insert(0, unit_id)

                            self.byteorder_drop.insert(0, bytes)

                            self.baudrate_entry.delete(0, END)
                            self.baudrate_entry.insert(0, baudrate)

                            self.bits_entry.delete(0, END)
                            self.bits_entry.insert(0, bits)

                            self.parity_entry.delete(0, END)
                            self.parity_entry.insert(0, parity)

                            self.rtu_bin_entry.delete(0, END)
                            self.rtu_bin_entry.insert(0, binary)

                            self.sb_e.delete(0, END)
                            self.sb_e.insert(0, stop_bits)

                            self.dev_entry.insert(0, device_name)
                            yaml_list = []
                            for v in range(len(hex_addr_list)):
                                new_list = []
                                new_list.append(hex_addr_list[v])
                                new_list.append(function_id_list[v])
                                new_list.append(data_type_list[v])
                                new_list.append(reg_name_list[v])
                                new_list.append(desc_list[v])
                                yaml_list.append(new_list)
                                self.register_list.insert(END, new_list)
        else:
            self.templ_w.destroy()
            tkinter.messagebox.showerror(
                "Error", "Please select a template to edit.")

    def _reset_listb(self):
        log_func("User attempting to revert database to last saved state..")
        self.template_list.delete(0, END)
        data = self._modb_funcs.revert_mb()
        self._changes = False
        for row in data:
            dev = ''.join(row)
            write_dev_to_yaml = self.db.get_all_info(dev)
            self.template_list.insert(0, dev)
            with open(PATH + '/yaml_files/modbus_data.yaml', 'a') as f:
                yaml.dump(write_dev_to_yaml, f)

    def _callback(self, event):
        selection = event.widget.curselection()
        if selection:
            index = selection[0]
            data = event.widget.get(index)
            regs = self.db.get_regs(data)
            self.template_regs_box.delete(0, END)
            for i in regs[::-1]:
                self.template_regs_box.insert(0, i)

    def _on_closing_add_temp(self, temp_w):
        temp_w.destroy()
        try:
            self._add_regs_window.destroy()
        except:
            log_func("Add register window doesn't exist")

    def _shift_down(self, listbox, temp_list, name_entry, identity):
        selection = listbox.curselection()
        if selection:
            log_func("User attempting to shift register down..")
            self._changes = True
            index = selection[0]
            data = listbox.get(index)
            next_item = listbox.get(index+1)
            if next_item == '':
                listbox.delete(index)
                listbox.insert(0, data)
            else:
                listbox.delete(index)
                listbox.insert(index+1, data)
                listbox.select_set(index+1)
            if identity == 'temp_reg_box':
                template = temp_list.curselection()
                temp = temp_list.get(template[0])
            else:
                temp = str(name_entry.get())
            with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
                doc = yaml.full_load(f)
                for v, item in enumerate(doc):
                    if doc[v]['templates'][0]['template_name'] == temp:
                        for i, d in enumerate(doc[v]['registers'][0]['reg_name']):
                            if d == data[3]:
                                register = doc[v]['registers'][0]
                                register['reg_name'].pop(i)
                                register['function_id'].pop(i)
                                register['data_type'].pop(i)
                                register['hex_address'].pop(i)
                                register['reg_desc'].pop(i)
                                if next_item == '':
                                    register['reg_name'].insert(
                                        0, data[3])
                                    register['function_id'].insert(
                                        0, data[1])
                                    register['data_type'].insert(
                                        0, data[2])
                                    register['hex_address'].insert(
                                        0, data[0])
                                    register['reg_desc'].insert(
                                        0, data[4])
                                else:
                                    register['reg_name'].insert(
                                        index+1, data[3])
                                    register['function_id'].insert(
                                        index+1, data[1])
                                    register['data_type'].insert(
                                        index+1, data[2])
                                    register['hex_address'].insert(
                                        index+1, data[0])
                                    register['reg_desc'].insert(
                                        index+1, data[4])
                                with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as d_file:
                                    yaml.dump(doc, d_file)

    def _shift_up(self, listbox, temp_list, name_entry, identity):
        selection = listbox.curselection()
        if selection:
            log_func("User attempting to shift register up..")
            self._changes = True
            index = selection[0]
            data = listbox.get(index)
            if index == 0:
                listbox.delete(index)
                listbox.insert(END, data)
            else:
                listbox.delete(index)
                listbox.insert(index-1, data)
                listbox.select_set(index-1)
            if identity == 'temp_reg_box':
                sel = temp_list.curselection()
                template = temp_list.get(sel[0])
            else:
                template = str(name_entry.get())
            with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
                doc = yaml.full_load(f)
                for v, item in enumerate(doc):
                    if doc[v]['templates'][0]['template_name'] == template:
                        for i, d in enumerate(doc[v]['registers'][0]['reg_name']):
                            if d == data[3]:
                                register = doc[v]['registers'][0]
                                register['reg_name'].pop(i)
                                register['function_id'].pop(i)
                                register['data_type'].pop(i)
                                register['hex_address'].pop(i)
                                register['reg_desc'].pop(i)
                                if index == 0:
                                    register['reg_name'].insert(
                                        len(register['reg_name']), data[3])
                                    register['function_id'].insert(
                                        len(register['reg_name']), data[1])
                                    register['data_type'].insert(
                                        len(register['reg_name']), data[2])
                                    register['hex_address'].insert(
                                        len(register['reg_name']), data[0])
                                    register['reg_desc'].insert(
                                        len(register['reg_name']), data[4])
                                else:
                                    register['reg_name'].insert(
                                        index-1, data[3])
                                    register['function_id'].insert(
                                        index-1, data[1])
                                    register['data_type'].insert(
                                        index-1, data[2])
                                    register['hex_address'].insert(
                                        index-1, data[0])
                                    register['reg_desc'].insert(
                                        index-1, data[4])
                                with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as d_file:
                                    yaml.dump(doc, d_file)

    def _del_reg_tmp(self, listbox, copy, temp_list,
                     name_entry, identity):
        selection = listbox.curselection()
        if selection:
            log_func("User attempting to delete modbus register from template..")
            self._changes = True
            index = selection[0]
            reg = listbox.get(index)
            if identity == 'temp_reg_box':
                template = temp_list.curselection()
                temp = temp_list.get(template)
            else:
                temp = str(name_entry.get())
            listbox.delete(first=index, last=index)
            self._modb_funcs.delete_temp_reg(temp, reg, copy)

    def _save_edit(self, copy, window):
        log_func("User attempting to save template..")
        uid_list = []
        uids = self.db.cur.execute(GET_UNIT_IDS)
        for uid in uids:
            uid_list.append(uid)
        self._changes = True
        if self.name_entry.get() and self.desc_entry.get() and self.byteorder_drop.get() and self.unit_spinbox.get() and self.register_list.get(0, END) and self.baudrate_entry.get() and self.bits_entry.get() and self.parity_entry.get() and self.rtu_bin_entry.get() and self.dev_entry.get() and self.sb_e.get():
            dev_name_limit = len(str(self.dev_entry.get()))
            exists = [ui for ui in uid_list if str(ui[0])
                      == self.unit_spinbox.get()]
            if dev_name_limit > 4:
                tkinter.messagebox.showerror(
                    "Error", "Device Name must be 4 characters or less.", parent=window)
            else:
                if copy != None:
                    devices_dict = [{
                        'templates_to_del': {'template': []}
                    }]
                    index = self.edited_selection[0]
                    edited_template = self.template_list.get(index)
                    edited_temp = None
                    edited_temp_id = None
                    doc = None
                    edited_temp_id = self.db.cur.execute(
                        GET_TEMP_ID(edited_template))
                    fetched = edited_temp_id.fetchone()
                    if fetched:
                        edited_temp = fetched[0]
                    if copy == 'edit':
                        self._modb_funcs.save_edit_yaml(
                            copy, edited_template, devices_dict, edited_temp)
                        self.template_list.delete(index)
                    with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
                        doc = yaml.full_load(f)
                        if doc:
                            for y, item in enumerate(doc):
                                if item['templates'][0]['template_name'] == str(self.name_entry.get()):
                                    del doc[y]
                                    if len(doc) == 0:
                                        del doc
                                    with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                                        if f:
                                            yaml.dump(doc, f)
                if len(exists) == 0 or exists[0][1] == str(self.dev_entry.get()):
                    self._send_to_yaml(window)
                else:
                    tkinter.messagebox.showerror(
                        "Error", "That unit ID is already taken.", parent=window)
        else:
            tkinter.messagebox.showerror(
                "Error", "You must enter a value for all fields.", parent=window)
        return self._changes

    def _add_template_w(self, copy):
        self.templ_w = Toplevel(self)
        self.templ_w.title("Add New Template")
        self.templ_w.geometry("765x400")
        self.templ_w.configure(bg=IVORY)
        self.templ_w.protocol("WM_DELETE_WINDOW",
                              lambda:
                              self._on_closing_add_temp(
                                  self.templ_w))
        log_func("User attempting to edit or add new template..")
        temp_name = Label(self.templ_w, text="Template Name:",
                          font=FONT, bg=IVORY)
        temp_name.grid(column=0, row=0)

        self.name_entry = Entry(self.templ_w, bg=IVORY)
        self.name_entry.grid(column=1, row=0)

        unit_label = Label(self.templ_w, text="Unit ID:",
                           font=FONT, bg=IVORY)
        unit_label.grid(column=2, row=0)

        self.unit_spinbox = Spinbox(self.templ_w,
                                    from_=0, to=100)
        self.unit_spinbox.grid(column=3, row=0)

        byte_label = Label(self.templ_w, text="Byte Order",
                           font=FONT, bg=IVORY)
        byte_label.grid(column=0, row=1)

        self.byteorder_drop = Combobox(self.templ_w)
        self.byteorder_drop.grid(column=1, row=1)
        self.byteorder_drop['values'] = ('MSB MSW',
                                         'MSB LSW',
                                         'LSB MSW',
                                         'LSB LSW')

        desc_label = Label(self.templ_w, text="Description:",
                           font=FONT, bg=IVORY)
        desc_label.grid(column=2, row=1)

        self.desc_entry = Entry(self.templ_w, bg=IVORY)
        self.desc_entry.grid(column=3, row=1)

        baud_label = Label(self.templ_w, text="Baudrate:",
                           font=FONT, bg=IVORY)
        baud_label.grid(column=0, row=2)

        self.baudrate_entry = Combobox(self.templ_w)
        self.baudrate_entry.grid(column=1, row=2)
        self.baudrate_entry['values'] = (1200, 2400, 4800,
                                         9600, 14400, 19200,
                                         28800, 38400, 57600,
                                         76800, 115200)
        self.baudrate_entry.set(9600)

        bit_label = Label(self.templ_w, text="Character Bits",
                          font=FONT, bg=IVORY)
        bit_label.grid(column=2, row=2)

        my_str_var = StringVar(self.templ_w)
        my_str_var.set("8")
        self.bits_entry = Spinbox(
            self.templ_w,  from_=0, to=10000, textvariable=my_str_var)
        self.bits_entry.grid(column=3, row=2)

        parity_label = Label(self.templ_w, text="Parity:",
                             font=FONT, bg=IVORY)
        parity_label.grid(column=0, row=3)

        self.parity_entry = Combobox(self.templ_w)
        self.parity_entry.grid(column=1, row=3)
        self.parity_entry['values'] = ('E', 'N', 'O')
        self.parity_entry.set('N')

        rtu_bin_label = Label(self.templ_w, text="Mode",
                              font=FONT, bg=IVORY)
        rtu_bin_label.grid(column=2, row=3)

        self.rtu_bin_entry = Combobox(self.templ_w)
        self.rtu_bin_entry.grid(column=3, row=3)
        self.rtu_bin_entry['values'] = ('RTU')
        self.rtu_bin_entry.set('RTU')

        dev_label = Label(self.templ_w, text="Device Name",
                          font=FONT, bg=IVORY)
        dev_label.grid(column=0, row=4)

        self.dev_entry = Entry(self.templ_w, bg=IVORY)
        self.dev_entry.grid(column=1, row=4)

        stop_bits_label = Label(self.templ_w, text="Stop Bits",
                                font=FONT, bg=IVORY)
        stop_bits_label.grid(column=2, row=4)

        self.sb_e = Combobox(self.templ_w)
        self.sb_e.grid(column=3, row=4)
        self.sb_e['values'] = (1, 2)
        self.sb_e.set(1)

        register_label = Label(self.templ_w, text="Registers",
                               font=FONT, bg=IVORY)
        register_label.grid(column=1, columnspan=2, row=5)

        self.register_list = Listbox(self.templ_w)
        self.register_list.grid(column=1, columnspan=2,
                                row=6, rowspan=5, ipadx=150)

        del_register_button = Button(self.templ_w, text="Delete",
                                     width=5, command=lambda:
                                     self._del_reg_tmp(self.register_list,
                                                       copy, self.template_list,
                                                       self.name_entry,
                                                       'temp_reg_box'),
                                     bg=IVORY, fg=BLACK, font=FONT,
                                     activebackground="green",
                                     activeforeground=IVORY)
        del_register_button.grid(column=3, row=6)

        up_btn = Button(self.templ_w, text="▲", width=5,
                        command=lambda:
                        self._shift_up(self.register_list,
                                       self.template_list,
                                       self.name_entry,
                                       None),
                        bg=IVORY, fg=BLACK,
                        activebackground="green", activeforeground=IVORY)
        up_btn.grid(column=3, row=7)

        down_btn = Button(self.templ_w, text="▼", width=5,
                          command=lambda:
                          self._shift_down(self.register_list,
                                           self.template_list,
                                           self.name_entry,
                                           None),
                          bg=IVORY, fg=BLACK,
                          activebackground="green", activeforeground=IVORY)
        down_btn.grid(column=3, row=8)

        add_register_button = Button(self.templ_w, text="Add",
                                     command=lambda:
                                     self._add_regs(
                                         self.register_list),
                                     width=5,
                                     bg=IVORY, fg=BLACK, font=FONT,
                                     activebackground="green", activeforeground=IVORY)
        add_register_button.grid(column=3, row=9)

        save_button = Button(self.templ_w, text="Save",
                             command=lambda:
                             self._save_edit(copy, self.templ_w),
                             bg=IVORY, fg=BLACK, font=FONT,
                             activebackground="green", activeforeground=IVORY)
        save_button.grid(column=1, row=11, ipadx=50)

        cancel_btn = Button(self.templ_w, text="Cancel", width=5,
                            command=lambda: self._cancel_operation(
                                self.templ_w),
                            bg=IVORY, fg=BLACK, font=FONT,
                            activebackground="green", activeforeground=IVORY)
        cancel_btn.grid(column=2, row=11, ipadx=50)

        if copy:
            self._edit_template(copy)
        if copy == ' (copy)':
            self.templ_w.title("Copied Template")
        elif copy == 'edit':
            self.templ_w.title("Edited Template")

    def _cancel_operation(self, curr_window):
        log_func("User attempting to cancel adding template..")
        curr_window.destroy()

    def _add_regs(self, reg_list):
        self._add_regs_window = Toplevel()
        self._add_regs_window.title("Add Registers")
        self._add_regs_window.geometry("725x480")
        self._add_regs_window.configure(bg="#FAF9F6")
        reg_tab = [['Hex Address', 'Function id',
                    'Data Type', 'Name', 'Description']]
        for item in reg_list.get(0, END):
            reg_tab.append(list(item))
        for item in reg_tab:
            if len(reg_tab) < 13:
                reg_tab.append(['', '', '', '', ''])
            else:
                pass
        total_rows = len(reg_tab)
        total_columns = len(reg_tab[0])
        for i in range(total_rows):
            for j in range(total_columns):
                if i == 0:
                    reg_entry = Entry(self._add_regs_window,
                                      width=15, fg='black', bg=IVORY)
                    reg_entry.grid(row=i, column=j)
                    reg_entry.insert(END, reg_tab[i][j])
                    reg_entry.configure(state='readonly')
                elif j == 1:
                    reg_entry = Combobox(self._add_regs_window, width=15)
                    reg_entry.grid(row=i, column=j)
                    reg_entry.insert(END, reg_tab[i][j])
                    reg_entry['values'] = (3, 4)
                else:
                    reg_entry = Entry(self._add_regs_window,
                                      width=15, fg='black', bg=IVORY)
                    reg_entry.grid(row=i, column=j)
                    reg_entry.insert(END, reg_tab[i][j])
        save_reg_button = Button(self._add_regs_window, text="Save",
                                 command=lambda: self._save_regs(
                                     reg_list),  bg=IVORY, fg=BLACK, font=FONT,
                                 activebackground="green", activeforeground=IVORY)
        save_reg_button.grid(column=1, row=i+1, ipadx=50)

        cancel_reg_btn = Button(self._add_regs_window, text="Cancel",
                                command=lambda: self._cancel_operation(
                                    self._add_regs_window),
                                bg=IVORY, fg=BLACK, font=FONT,
                                activebackground="green", activeforeground=IVORY)
        cancel_reg_btn.grid(column=3, row=i+1, ipadx=50)

    def _send_to_yaml(self, curr_window):
        yaml_template_name = self._modb_funcs.save_to_yaml(
            str(self.name_entry.get()),
            str(self.desc_entry.get()),
            str(self.byteorder_drop.get()),
            str(self.unit_spinbox.get()),
            list(self.register_list.get(0, END)),
            str(self.baudrate_entry.get()),
            str(self.bits_entry.get()),
            str(self.parity_entry.get()),
            str(self.rtu_bin_entry.get()),
            str(self.dev_entry.get()),
            str(self.sb_e.get())
        )
        for i, item in enumerate(self.template_list.get(0, END)):
            if item == yaml_template_name:
                self.template_list.delete(i)
        self.template_list.insert(0, yaml_template_name)
        curr_window.destroy()

    def _save_regs(self, register_list):
        log_func("User attempting to save registers to template..")
        regs = []
        row = []
        for child in self._add_regs_window.children.values():
            if type(child) == Entry and child.get() or type(child) == Combobox and child.get():
                val = child.get()
                row.append(val)
                if len(row) % 5 == 0:
                    regs.append(tuple(row))
                    row.clear()
        sawn_off_regs = regs[1:]
        register_list.delete(0, END)
        for i, reg_str in enumerate(sawn_off_regs):
            len_str = len(sawn_off_regs[i][3])
            if len_str > 4:
                tkinter.messagebox.showerror(
                    "Error", "Register must be 4 characters or less.",
                    parent=self._add_regs_window)
                return
            register_list.insert(END, reg_str)
        self._add_regs_window.destroy()
        return regs

    def _write_to_dev(self, temp_list):
        selection = temp_list.curselection()
        regs = None
        if selection:
            index = selection[0]
            cancel = tkinter.messagebox.askyesnocancel(
                "Save", "Write this template to your device?")
            if cancel:
                log_func("User attempting to write modbus template to sensor..")
                chosen_template = temp_list.get(index)
                curr_devs = []
                self._modbus = self._dev.get_modbus()
                if self._modbus.devices:
                    for dev in self._modbus.devices:
                        curr_devs += [ dev.unit ]
                regs = self.db.get_modbus_template_regs(chosen_template)
                if regs:
                    curr_regs = []
                    for dev in self._modbus.devices:
                        for reg in dev.regs:
                            curr_regs += [ reg.name ]
                    for reg in regs:
                        unit_id, bytes_fmt, hex_addr, func_type, \
                            data_type, reg_name, dev_name = reg
                        if unit_id not in curr_devs:
                            self._dev.modbus_dev_add(unit_id,
                                                     dev_name,
                                                     "MSB" in bytes_fmt,
                                                     "MSW" in bytes_fmt)
                            curr_devs.append(unit_id)
                        if reg_name not in curr_regs:
                            self._dev.modbus_reg_add(unit_id,
                                                     binding.modbus_reg_t(self._dev,
                                                                  reg_name,
                                                                  int(hex_addr, 16),
                                                                  func_type,
                                                                  data_type,
                                                                  reg_name))
                    self._load_modbus(True)
                    tkinter.messagebox.showinfo(
                        "Device Written", "Go to debug mode to monitor data.")
                else:
                    tkinter.messagebox.showerror(
                        "Error", "Device must be saved.", parent=root)
        else:
            tkinter.messagebox.showerror(
                "Error", "Select a template to write to device.", parent=root)
