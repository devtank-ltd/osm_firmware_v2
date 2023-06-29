#!/usr/bin/python3

import os
from tkinter import *
import tkinter.messagebox
from tkinter.ttk import Combobox, Notebook, Progressbar, Style
import webbrowser
import re
import yaml
from idlelib.tooltip import Hovertip
from tkinter.filedialog import askopenfilename
import threading
import traceback
import logging
import platform
import signal
from stat import *
import subprocess
import tempfile

from gui_binding_interface import binding_interface_client_t
from modbus_funcs import modbus_funcs_t
import modbus_db
from modbus_db import modb_database_t, find_path
import binding

import serial.tools.list_ports
import serial
from PIL import ImageTk, Image

FW_PROCESS = False
THREAD = threading.Thread

LINUX_OSM_TTY = "/tmp/osm/UART_DEBUG_slave"

PATH = find_path()
NAVY = "#292924"
IVORY = "#FAF9F6"
CHARCOAL = "#565656"
DRK_GRN = "#17632f"
LIME_GRN = "#00ff41"
BLACK = "#050505"
BLUE = "#151199"
RED = "#9c091d"
OSM_GRN = "#7ad03a"
FONT = ('Arial', 11, 'bold')
FONT_L = ('Arial', 14, 'bold')
FONT_XL = ('Arial', 20, 'bold')
FONT_XXL = ('Karumbi', 35, 'bold')
ICONS_T   =    PATH + "/osm_pictures/logos/icons-together.png"
DVT_IMG   =    PATH + "/osm_pictures/logos/OSM+Powered.png"
OSM_1     =    PATH + "/osm_pictures/logos/Lora-Rev-C.png"
R_LOGO    =    PATH + "/osm_pictures/logos/shuffle.png"
GRPH_BG   =    PATH + "/osm_pictures/logos/graph.png"
PARAMS    =    PATH + "/osm_pictures/logos/parameters.png"
OPEN_S    =    PATH + "/osm_pictures/logos/opensource-nb.png"
OSM_BG    =    PATH + "/osm_pictures/logos/leaves.jpg"


def log_func(msg):
    logging.info(msg)


def open_url(url):
    webbrowser.open_new(url)


def check():
    root.after(500, check)

def handle_exit(sig, frame):
    log_func("Ctrl-c detected, exiting..")
    exit()


class config_gui_window_t(Tk):
    def __init__(self):
        super().__init__()
        signal.signal(signal.SIGINT, handle_exit)
        log_func(f"current path: {PATH}")
        try:
            self.db = modb_database_t(PATH)
        except Exception as e:
            log_func(f"Could not connect to db: {e}")
        self._modb_funcs = modbus_funcs_t(self.db)
        self._connected = False
        self._changes = False
        self._widg_del = False
        with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
            pass
        with open(PATH + '/yaml_files/del_file.yaml', 'w') as df:
            pass

        self.binding_interface = binding_interface_client_t()

        style = Style()
        style.configure('lefttab.TNotebook', tabposition='wn',
                        background="green")
        style.map("lefttab.TNotebook.Tab", background=[
            ("selected", IVORY)], foreground=[("selected", BLACK)])
        style.configure('lefttab.TNotebook.Tab', background="green",
                        foreground=IVORY, font=FONT, width=15,
                        padding=10)
        style.configure("Tab", focuscolor=style.configure(".")["background"])
        self._notebook = Notebook(self, style='lefttab.TNotebook')
        self._notebook.pack(expand=True, fill='both')

        self._conn_fr = Frame(self._notebook, width=1400, height=1000,
                              bg=IVORY, padx=50, pady=50)
        self._conn_fr.pack(fill='both', expand=True)
        self._conn_fr.pack_propagate(0)

        self._main_fr = Frame(self._notebook,
                              bg=IVORY, padx=50)
        self._main_fr.pack(fill='both', expand=True)
        self._main_fr.pack_propagate(0)

        self._adv_fr = Frame(self._notebook, bg=IVORY)
        self._adv_fr.pack(fill='both', expand=True)
        self._adv_fr.pack_propagate(0)

        self._modb_fr = Frame(self._notebook,
                              bg=IVORY, padx=50, pady=50)
        self._modb_fr.pack(fill='both', expand=True)
        self._modb_fr.pack_propagate(0)

        usb_label = Label(self._conn_fr, text="Select a device",
                          bg=IVORY, font=FONT)
        usb_label.pack()
        returned_ports = []
        curr_platform = "COM" if platform.system(
        ) == "Windows" else "ttyUSB" if platform.system(
        ) == "Linux" else "cu.usbserial" if platform.system(
        ) == "Darwin" else None
        active_ports = serial.tools.list_ports.comports(include_links=True)
        for item in active_ports:
            if curr_platform in item:
                returned_ports.append(item.device)
        if os.path.exists(LINUX_OSM_TTY):
            returned_ports += [ LINUX_OSM_TTY ]
        dropdown = Combobox(self._conn_fr, values=returned_ports,
                            font=FONT, width=22)
        for item in active_ports:
            if curr_platform in item.device:
                dropdown.set(item.device)
        dropdown.pack()

        self._dev_dropdown = dropdown
        self.dev_sel = dropdown.get()

        self._connect_btn = Button(self._conn_fr, text="Connect",
                                   command=self._on_connect,
                                   bg=IVORY, fg=BLACK, font=FONT,
                                   width=20, activebackground="green",
                                   activeforeground=IVORY)
        self._connect_btn.pack()

        self._downl_fw = Button(self._conn_fr, text="Update Firmware",
                                command=lambda: self._update_fw(
                                    self._conn_fr), bg=IVORY, fg=BLACK,
                                font=FONT, width=20, activebackground="green",
                                activeforeground=IVORY)
        self._downl_fw.pack()
        fw_hover = Hovertip(
                self._downl_fw, "Choose a file to update the sensor's firmware with. (This can only be done before connecting)")
        self._progress = Progressbar(
            self._conn_fr, orient=HORIZONTAL, length=100, mode='determinate', maximum=150)
        self._progress.pack()
        self._progress.pack_forget()
        self._fw_label = Label(self._conn_fr, text="", bg=IVORY)
        self._fw_label.pack()

        self._welcome_p = Label(self._conn_fr, text="Welcome!\n\nOpen Smart Monitor Configuration",
                                font=FONT_XL, pady=50, bg=IVORY)
        self._welcome_p.pack()

        conn_canv = Canvas(self._conn_fr)
        conn_canv.pack()

        img = Image.open(OSM_1)
        osm1_logo = ImageTk.PhotoImage(img)
        self._conn_fr.img_list = []
        self._conn_fr.img_list.append(osm1_logo)
        self._osm1_lab = Label(
            conn_canv, image=osm1_logo, bg=IVORY,
            width=792, height=400)
        self._osm1_lab.pack()
        conn_canv.bind('<Configure>', lambda e: self._resize_image(
                        e, self._osm1_lab, img, self._conn_fr
                    ))
        self._check_binding_comms()

    def _check_binding_comms(self):
        count = 0
        while self.binding_interface.process_message() != None and count < 10:
            count+=1
        self.after(100, lambda: self._check_binding_comms())

    def _on_connect(self):
        self.dev_sel = self._dev_dropdown.get()
        if self.dev_sel:
            log_func("User attempting to connect to device.. : " + self.dev_sel)
            self._downl_fw.configure(state='disabled')
            if self._connected == True:
                self._connected = False
                self._widg_del = True
                tabs = self._notebook.winfo_children()
                for i, v in enumerate(tabs):
                    for t in tabs[i].winfo_children():
                        if i != 0:
                            t.destroy()
            self._populate_tabs()
        else:
            log_func("No device to connect to.")

    def _populate_tabs(self):
        if self._connected == True:
            self._on_connect()
        else:
            if self.dev_sel:
                self.binding_interface.open(self.dev_sel, self._on_open_done_cb)

    def _on_open_done_cb(self, resp):
        success = resp[1]
        if not success:
            return
        self._connected = True
        if self._widg_del == False:
            self._notebook.add(self._conn_fr, text="Connect",)
            self._notebook.add(self._main_fr, text="Home Page")
            self._notebook.add(
                self._adv_fr, text='Advanced config')
            self._notebook.add(
                self._modb_fr, text="Modbus config")

        self._notebook.select(1)
        self._sensor_name = Label(self._main_fr, text="",
                                    bg=IVORY)
        self._sensor_name.grid(
            column=4, row=0, columnspan=3, sticky='E')

        self._fw_version_body = Label(self._main_fr, text="")
        self._fw_version_body.grid(
                                column=5, row=1,
                                columnspan=2,
                                pady=(0, 50), sticky='E')

        self._load_config_btn = Button(self._main_fr, text="Load Config",
                                       font=FONT, bg=IVORY,
                                       command= self._open_json_config_file)
        self._load_config_btn.grid(column=0, row=0, sticky='W', pady=(10, 0))

        self._save_config_btn = Button(self._main_fr, text="Save Config",
                                       font=FONT, bg=IVORY,
                                       command = self._save_config_to_json)
        self._save_config_btn.grid(column=1, row=0, sticky='W', pady=(10, 0))

        self.save_load_label = Label(self._main_fr, text="",
                                     font=FONT, bg=IVORY)
        self.save_load_label.grid(column=2, row=0, sticky='W', pady=(10, 0))

        self._load_meas_l = Label(
            self._main_fr,
            text="Current measurements on OSM",
            font=FONT_L, bg=IVORY)
        self._load_meas_l.grid(
            column=1, row=5)

        logo_canv = Canvas(self._main_fr)
        logo_canv.grid(column=6, row=10, rowspan=5)

        self._main_fr.img_list = []
        img = Image.open(DVT_IMG)
        dev_logo = ImageTk.PhotoImage(img)
        self._main_fr.img_list.append(dev_logo)
        dev_lab = Label(logo_canv, image=dev_logo,
            bg=IVORY, width=403, height=176)
        dev_lab.grid(column=0, row=0, sticky=NSEW)
        logo_canv.bind('<Configure>', lambda e: self._resize_image(
            e, dev_lab, img, self._main_fr
        ))

        web_url = Label(
            self._main_fr,
            text="Click here to visit our website!",
            font=FONT_XXL, bg=IVORY, fg=OSM_GRN)
        web_url.grid(column=6, row=15, columnspan=2,
        sticky=EW)
        web_url.bind(
            "<Button-1>",
            lambda e: open_url("https://www.devtank.co.uk"))
        web_url.configure(cursor='hand2')

        banner_fr = Canvas(self._main_fr, bg=IVORY)
        banner_fr.grid(column=0, row=17, columnspan=7)
        img3 = Image.open(ICONS_T)
        emc_img = ImageTk.PhotoImage(img3)
        emc_lab = Label(banner_fr, image=emc_img, bg=IVORY,
                width=1065, height=111)
        emc_lab.grid(column=0, row=0)
        self._main_fr.img_list.append(emc_img)
        banner_fr.bind('<Configure>', lambda e: self._resize_image(
            e, emc_lab, img3, self._main_fr
        ))

        img_gr = Image.open(GRPH_BG)
        gra_logo = ImageTk.PhotoImage(img_gr)
        self._adv_fr.img_list = []
        self._adv_fr.img_list.append(gra_logo)
        self._gra_lab = Label(
            self._adv_fr, image=gra_logo, bg=IVORY)
        self._gra_lab.place(x=0, y=0, relwidth=1.0, relheight=1.0)
        self._adv_fr.bind('<Configure>', lambda e: self._resize_image(
            e, self._gra_lab, img_gr, self._adv_fr
        ))


        self._terminal = Text(
            self._adv_fr, bg=BLACK, fg=LIME_GRN,
            borderwidth=10, relief="sunken")
        self._terminal.pack(expand=True, fill='both',
            pady=(50,0), padx=200)
        self._terminal.configure(state='disabled')

        cmd = Entry(self._adv_fr,fg=CHARCOAL,
                    font=FONT_L)
        cmd.pack(expand=True, fill='x', padx=200)
        cmd.insert(0, 'Enter a command and hit return to send.')
        cmd.bind("<Return>", lambda e: self._enter_cmd(cmd))
        cmd.bind("<Button-1>",
                    lambda event:  self._clear_box(event, cmd))

        self._open_lora_config(self._main_fr)

        self.modbus_opened = False

        self._notebook.bind("<<NotebookTabChanged>>",
                            lambda e: self._tab_changed(e,
                                                        self._main_fr,
                                                        self._notebook))


        self._modbus        = None
        self._sens_meas     = None
        self.ser_op         = None
        self.fw_version     = None
        self.dev_eui        = None
        self.app_key        = None
        self._interval_min  = None
        self.cc_gain        = None

        self._load_gui()

    def _load_gui(self):
        self.binding_interface.get_interval_min(self._on_get_interval_min_done_cb)
        self.binding_interface.get_measurements(self._on_get_measurements_done_cb)
        self.binding_interface.get_serial_num(self._on_get_serial_num_done_cb)
        self.binding_interface.get_fw_version(self._on_get_fw_version_done_cb)
        self.binding_interface.get_dev_eui(self._on_get_dev_eui_done_cb)
        self.binding_interface.get_app_key(self._on_get_app_key_done_cb)
        self.binding_interface.get_comms_conn(self._on_get_comms_conn_done_cb)
        self.binding_interface.get_modbus(self._on_get_modbus_done_cb)
        self.binding_interface.print_cc_gain(self._on_get_cc_gain_cb)

    def _on_get_ftma_specs(self, resp):
       self.ftma_specs = resp[1]
       self._load_headers(self._main_fr, "rif")
       return self.ftma_specs

    def _on_update_cc_gain_cb(self, resp):
        self.binding_interface.print_cc_gain(self._on_open_cc_cb)

    def _on_get_cc_gain_cb(self, resp):
        self.cc_gain = resp[1]
        return self.cc_gain

    def _on_open_cc_cb(self, resp):
        self.cc_gain = resp[1]
        self._fill_cc_term()
        return self.cc_gain

    def _on_get_cmd_multi_done_cb(self, resp):
        cmd = resp[1]
        if cmd:
            for cm in cmd:
                self._write_terminal_cmd(cm, self._terminal)

    def _on_get_modbus_done_cb(self, resp):
        self._modbus = resp[1]
        self._load_modbus()

    def _on_remove_mb_reg(self, resp):
        self._modbus = resp[1]
        list_of_devs = []
        self._load_modbus()
        for i in range(len(self.mb_entries)):
            dev_entry = self.mb_entries[i][0]
            dev = dev_entry.get()
            list_of_devs.append(dev)
        if self.dev_to_remove not in list_of_devs:
            self.binding_interface.modbus_dev_del(self.dev_to_remove)

    def _on_get_measurements_done_cb(self, resp):
        self._sens_meas = resp[1]
        self.meas_headers = [i[0] for i in self._sens_meas]
        self.binding_interface.get_ftma_specs(self.meas_headers, self._on_get_ftma_specs)
        return self._sens_meas

    def _on_get_comms_conn_done_cb(self, resp):
        conn = resp[1]
        if conn is not None:
            if conn:
                self._lora_status.configure(text="Connected", fg="green")
            else:
                self._lora_status.configure(text="Disconnected", fg="red")
        else:
            self._lora_status.configure(text="Disconnected", fg="red")

    def _on_get_serial_num_done_cb(self, resp):
        self.ser_op = resp[1]
        self._sensor_name.configure(
            text="Serial number - " + self.ser_op,
            bg=IVORY, font=FONT)
        pass

    def _on_get_fw_version_done_cb(self, resp):
        fw_version = resp[1]
        self.fw_version = "-".join(fw_version.split("-")[0:2])
        self._fw_version_body.configure(
            text="Current firmware version - " + self.fw_version,
            bg=IVORY, font=FONT)

    def _on_get_dev_eui_done_cb(self, resp):
        self.dev_eui = resp[1]
        self._eui_entry.delete(0, END)
        self._eui_entry.insert(0, self.dev_eui)

    def _on_get_app_key_done_cb(self, resp):
        self.app_key = resp[1]
        self._app_entry.delete(0, END)
        self._app_entry.insert(0, self.app_key)

    def _on_get_interval_min_done_cb(self, resp):
        self._interval_min = resp[1]
        return self._interval_min

    def _on_get_interval_update_done_cb(self, resp):
        self._interval_min = resp[1]
        self._load_headers(self._main_fr, "rif")

    def _on_write_json_to_osm(self, resp):
        self.save_load_label.configure(text="Config loaded. Reconnecting..")
        self._on_connect()

    def _on_save_config_json(self, resp):
        loc = resp[1]
        log_func(f"OSM configuration saved in {loc}")
        self.save_load_label.configure(text="Config saved.")

    def _save_config_to_json(self):
        serial = "unknown"
        if self.ser_op:
            s = self.ser_op.split("-")[-1]
            match = re.findall(r"0*([0-9]+)", s)
            serial = match[0]
        filepath = tempfile.NamedTemporaryFile(prefix=f'osm_sensor_{serial}_',suffix='.json', delete=False)
        self.binding_interface.save_config_to_json(filepath.name, self._on_save_config_json)

    def _tab_changed(self, event, frame, notebook):
        slction = notebook.select()
        log_func(f"User changed to tab {slction}.")
        if slction == '.!notebook.!frame4' and self.modbus_opened == False:
            with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                pass
            self.modbus_opened = True
            self.main_modbus_w()

    def _clear_box(self, event, entry):
        entry.delete(0, END)
        return

    def _open_json_config_file(self):
        filetypes = ('json files', '*.json')
        selected = askopenfilename(filetypes=[filetypes])
        if selected:
            if tkinter.messagebox.askyesnocancel("Update?", "Update sensor with config from this file?",
                                                    parent=root):
                self._write_json_config(self._main_fr, selected)

    def _write_json_config(self, frame, filename):
        self._visit_widgets(self._main_fr, 'disabled')
        self.binding_interface.write_json_to_osm(filename, self._on_write_json_to_osm)

    def _visit_widgets(self, frame, cmd):
        meas = None
        # Disable tabs
        if frame == self._notebook:
            for i in range(0, 5):
                if i != 1:
                    self._notebook.tab(i, state=cmd)
        # Disable widgets in frame
        for child in frame.winfo_children():
            widg_n = child.widgetName
            if widg_n == 'button' or widg_n == 'entry':
                child.configure(state=cmd)
            if frame == self._main_fr:
                for i in self._second_frame.winfo_children():
                    i.configure(state=cmd)

    def _start_fw_cmd(self, selected, frame):
        log_func("User attempting to flash firmware to sensor...")
        global FW_PROCESS
        FW_PROCESS = False
        self._fw_label.configure(text="")
        self._welcome_p.pack_forget()
        self._osm1_lab.pack_forget()
        self._progress.pack()
        self._welcome_p.pack()
        self._osm1_lab.pack()
        self._visit_widgets(self._conn_fr, 'disabled')
        self._cmd = None
        try:
            self._cmd = subprocess.Popen(
                ["sudo", PATH + "/static_program.sh", selected], shell=False)
        except Exception as e:
            self._stop()
            traceback.print_exc()
            log_func(f"Subprocess failed : {e}")
        if self._cmd:
            self._step()

    def _stop(self):
        global FW_PROCESS
        FW_PROCESS = True
        self._result()

    def _step(self, count=0):
        self._progress['value'] = count
        done = self._cmd.poll()
        if FW_PROCESS == False:
            if done is not None:
                self._stop()
                return
        root.after(100)
        if count == 150:
            if done is None:
                self._step(1)
            else:
                self._stop()
                return
        root.after(1, lambda: self._step(count+1))

    def _result(self):
        self._cmd.returncode = None
        self._progress.pack_forget()

        if FW_PROCESS:
            self._visit_widgets(self._conn_fr, 'normal')
            if self._cmd == None or self._cmd.returncode != None:
                self._fw_label.configure(
                    text="Firmware failed to write.", font=FONT)
            elif self._cmd.returncode == None:
                self._fw_label.configure(
                    text="Firmware Written Successfully", font=FONT)

    def _update_fw(self, frame):
        filetypes = ('bin files', '*.bin')
        selected = askopenfilename(filetypes=[filetypes])
        if selected:
            if tkinter.messagebox.askyesnocancel("Write?", "Write latest software update to sensor?",
                                                 parent=root):
                self._start_fw_cmd(selected, frame)

    def _write_terminal_cmd(self, cmd, terminal):
        if cmd is None:
            cmd = ""
        terminal.configure(state='normal')
        terminal.insert(INSERT, cmd + "\n")
        terminal.configure(state='disabled')

    def _update_meas_tab(self, meas):
        for i in range(len(self._entries)):
            for row in self._entries[i]:
                g = row.get()
                if g == meas:
                    uplink_to_update = self._entries[i][1]
                    interval_to_update = self._entries[i][2]
        interval_to_update.delete(0, END)
        interval_to_update.insert(0, 0)
        uplink_to_update.delete(0, END)
        uplink_to_update.insert(0, 0)

    def _change_sample_interval(self, event):
        meas_chang = ""
        widget = event.widget
        widget_str = str(widget)
        length = len(widget_str)
        widget_num = widget_str[length - 2:]
        widget_val = widget.get()
        if int(widget_val) > 99:
            widget_val = '99'
        for i in range(len(self._entries)):
            for row in self._entries[i]:
                if str(row) == widget_str:
                    meas_chang = self._entries[i][0]
                    meas_chang = meas_chang.get()
                    inv_chang = self._entries[i][2]
                    uplink_chang = self._entries[i][1]
                    samp_chang = self._entries[i][3]
                    break
        widget_no = re.findall('\d', widget_num)
        widget_id = ''.join(widget_no)
        if int(widget_id) % 4 == 0:
            self.binding_interface.change_sample(meas_chang, widget_val)
            samp_chang.delete(0, END)
            samp_chang.insert(0, widget_val)
        elif int(widget_id) % 2 == 0:
            self.binding_interface.change_interval(meas_chang, widget_val)
            if meas_chang == 'TMP2' and widget_val != '0':
                self.binding_interface.change_interval("CNT1", "0")
                self._update_meas_tab('CNT1')
            elif meas_chang == 'CNT1' and widget_val != '0':
                self.binding_interface.change_interval("TMP2" ,"0")
                self._update_meas_tab('TMP2')
            res = int(self._interval_min) * int(widget_val)
            inv_chang.delete(0, END)
            inv_chang.insert(0, res)
            uplink_chang.delete(0, END)
            uplink_chang.insert(0, widget_val)
        else:
            self._change_mins(widget_val, inv_chang, uplink_chang, meas_chang)

    def _change_mins(self, widget_val, inv_chang, uplink_chang, meas_chang):
        if int(widget_val) < int(self._interval_min):
            widget_val = self._interval_min
            inv_chang.delete(0, END)
            inv_chang.insert(0, int(widget_val))
        elif int(widget_val) % 5 != 0:
            widget_val = self._round_to_multiple(
                int(widget_val), int(self._interval_min))
            inv_chang.delete(0, END)
            inv_chang.insert(0, int(widget_val))
        mins = int(widget_val) / int(self._interval_min)
        min = round(mins, 0)
        self.binding_interface.change_interval(meas_chang, min)
        uplink_chang.delete(0, END)
        uplink_chang.insert(0, int(min))
        # if user changes interval in mins for pulsecount or one wire
        if meas_chang == 'CNT1' and widget_val != '0':
            self.binding_interface.change_interval("TMP2", "0")
            self._update_meas_tab('TMP2')
        elif meas_chang == 'TMP2' and widget_val != '0':
            self.binding_interface.change_interval("CNT1", "0")
            self._update_meas_tab('CNT1')

    def _round_to_multiple(self, number, multiple):
        return multiple * round(number / multiple)


    def _get_ios_meas_inst(self, meas):
        if meas.startswith("TMP"):
            return 'w1'
        elif meas.startswith("CNT"):
            return 'pulse'

    def _get_ios_meas_desc(self, meas):
        if meas.startswith("TMP"):
            return 'One Wire Temperature'
        elif meas.startswith("CNT"):
            return 'Pulsecount'

    def on_get_ios_val(self, resp):
        self._rise_fall = Combobox(self.ios_page)
        self._rise_fall.pack()
        self._rise_fall['values'] = ('Rise', 'Fall', 'Both')
        self._rise_fall.set('Rise')
        pin_obj = None
        pin_is = resp[1][0]
        pull = resp[1][1]
        if not self.io_meas.startswith('TMP'):
            io_d_label = Label(
                self.ios_page,
                text="Set Pull Up or Pull Down")
            io_d_label.pack()
            self._up_d_non = Combobox(self.ios_page)
            self._up_d_non.pack()
            self._up_d_non['values'] = ('Up', 'Down', 'None')
            if self.io_meas == 'CNT1':
                self._up_d_non.set('Up')
                self._up_d_non.configure(state='disabled')
            else:
                if pull:
                    if pull[0] == 'u':
                        self._up_d_non.set('Up')
                    elif pull[0] == 'd':
                        self._up_d_non.set('Down')
                    else:
                        self._up_d_non.set('None')
                else:
                    self._up_d_non.set('None')
                if pin_is == self.io_meas:
                    self._up_d_non.configure(state='disabled')
        if pin_is == self.io_meas:
            dis_check = Button(self.ios_page,
                               text="Disable",
                               command=lambda: self._do_ios_cmd('disable', self.io_meas, pin_obj, pin_is),
                               bg=IVORY, fg=BLACK, font=FONT,
                               activebackground="green", activeforeground=IVORY)
            dis_check.pack()
        else:
            en_check = Button(self.ios_page,
                              text="Enable",
                              command=lambda: self._do_ios_cmd('enable', self.io_meas, pin_obj, pin_is),
                              bg=IVORY, fg=BLACK, font=FONT,
                              activebackground="green", activeforeground=IVORY)
            en_check.pack()
        self._ios_label = Label(self.ios_page, text="",
            bg=IVORY, font=FONT)
        self._ios_label.pack()
        self._set_ios_label(self.io_meas, pin_obj, pin_is)

    def _get_ios_pin_obj(self, meas):
        if meas == "TMP2" or meas == "CNT1":
            self.binding_interface.get_ios(1, self.on_get_ios_val)
        elif meas == "TMP3" or meas == "CNT2":
            self.binding_interface.get_ios(2, self.on_get_ios_val)
        return None

    def _set_ios_label(self, meas, pin_obj, pin_is):
        desc = self._get_ios_meas_desc(meas)

        if pin_is == meas:
            self._ios_label.configure(
                text=f"{desc} enabled.",
                bg=LIME_GRN, fg=BLACK)
        elif pin_is:
            self._ios_label.configure(
                text=f"That IO is in use as {self._get_ios_meas_desc(pin_is)}.",
                bg=RED, fg=IVORY)
        else:
            self._ios_label.configure(
                text=f"{desc} disabled.", bg=RED, fg=IVORY)

    def _do_ios_cmd(self, cmd, meas, pin_obj, pin_is):
        log_func(f"User attempting to {cmd} IO for {meas}")
        inst = self._get_ios_meas_inst(meas)
        desc = self._get_ios_meas_desc(meas)
        rise = self._rise_fall.get()
        if rise == 'Rise':
            rise = 'R'
        elif rise == 'Fall':
            rise = 'F'
        else:
            rise = 'B'
        if meas.startswith('CNT'):
            if meas == "CNT2":
                if self._up_d_non.get():
                    pullup = self._up_d_non.get()
                    if pullup == 'Up':
                        pullup = 'U'
                    elif pullup == 'Down':
                        pullup = 'D'
                    else:
                        pullup = 'N'
                else:
                    self._ios_label.configure(
                        text="Select a pull up.", bg=RED, fg=IVORY)
                    return
            else:
                pullup = "U"
        else:
            pullup = "N"
        if cmd == 'enable':
            # check if user is trying to overwrite existing io
            if pin_is:
                self.binding_interface.disable_io()
            self.binding_interface.activate_io(inst, rise, pullup)
        elif cmd == 'disable':
            # disable io
            self.binding_interface.disable_io()
        self.ios_page .destroy()
        self._open_ios_w(meas)


    def _open_ios_w(self, meas):
        self.ios_page = Toplevel(self.master)
        self.ios_page.title(f"Set IO {meas}")
        self.ios_page.geometry("405x500")
        self.ios_page.configure(bg=IVORY)
        log_func("User opened IO window..")
        self.io_meas = meas
        self._get_ios_pin_obj(meas)

    def _on_mousewheel(self, event, canvas):
        canvas.yview_scroll(-1*(event.delta/120), "units")

    def _get_desc(self, reg):
        d = self.db.get_meas_description(reg)
        if d is None:
            d = self.db.get_modbus_reg_desc(reg)
        if d != None and len(d[0]) > 1:
            return d[0]
        return d

    def _change_uplink(self, frame, widg):
        if int(widg.get()):
            uplink = int(widg.get())
            if uplink > 255:
                self.binding_interface.set_interval_mins(255, self._on_change_uplink)
            elif uplink < 3:
                self.binding_interface.set_interval_mins(3, self._on_change_uplink)
            else:
                self.binding_interface.set_interval_mins(uplink, self._on_change_uplink)
        else:
            tkinter.messagebox.showerror(
                "Error", "Uplink interval must be an integer")

    def _on_change_uplink(self, args):
        self.binding_interface.get_interval_min(self._on_get_interval_update_done_cb)

    def _handle_input(self, in_str, acttyp):
        if acttyp == '1':
            if not in_str.isdigit():
                return False
        return True

    def _load_headers(self, window, idy):
        tablist = [('Measurement', 'Uplink (%smin)' % self._interval_min,
                    'Interval in Mins', 'Sample Count', 'Value')]
        if self._sens_meas:
            pos = None
            for i in range(len(self._sens_meas)):
                row = self._sens_meas[i]
                for n in range(len(row)):
                    entry = row[n]
                    if n != 0 and type(entry) != int:
                        pos = entry.find("x")
                    if pos != -1:
                        row[n] = entry[0:pos]
                row.insert(2, (int(self._interval_min) * int(row[1])))
                row.insert(4, "")
                tablist.append(row)
        else:
            log_func("No measurements on sensor detected.")
        entry_change_uplink = Entry(
            window, fg=CHARCOAL, font=FONT_L)
        entry_change_uplink.grid(column=0, columnspan=5, row=15,
            sticky="NEW", padx=(0,50))
        entry_change_uplink.insert(
            0, 'Enter a number and hit return to set uplink.')
        entry_change_uplink.bind(
            "<Return>", lambda e: self._change_uplink(window,
                                                        entry_change_uplink))
        entry_change_uplink.bind(
            "<Button-1>", lambda event: self._clear_box(event,
                                                        entry_change_uplink))
        entry_change_uplink.configure(validate="key", validatecommand=(
            window.register(self._handle_input), '%P', '%d'))
        self._load_measurements(window, idy, tablist)

    def _on_canvas_config(self, e, canv):
        canv.configure(
            scrollregion=canv.bbox("all"))
        canv.itemconfig('frame', width=canv.winfo_width())

    def _load_modbus(self):
        modbus = self._modbus
        tablist = [('Device', 'Name', 'Hex Address',
                    'Data Type', 'Function')]
        for i in range(len(modbus)):
            hx = hex(int(modbus[i][2]))
            row = list(modbus[i])
            row[2] = hx
            tablist.append(row)
        self._mb_fr = Frame(self._modb_fr, borderwidth=8,
                                 relief="ridge", bg="green")
        self._modb_fr.columnconfigure([1,3,4], weight=1)
        self._modb_fr.rowconfigure([11,12,13,14,15,16,17], weight=1)
        self._mb_canvas = Canvas(
            self._mb_fr, bg=IVORY)
        self._mb_canvas.grid(column=0, row=0, sticky=NSEW)
        self._mb_fr.grid(
            column=4, row=1, rowspan=7,
            sticky=NSEW, padx=(50, 0))
        self._second_mb_fr = Frame(self._mb_canvas)
        self._second_mb_fr.pack(expand=True, fill='both')

        self._mb_fr.columnconfigure(0, weight=1)
        self._mb_fr.rowconfigure(0, weight=1)
        sb = Scrollbar(self._mb_fr, orient='vertical',
                       command=self._mb_canvas.yview)
        sb.grid(column=0, row=0, sticky="NSE")
        self._mb_canvas.configure(yscrollcommand=sb.set)
        self._mb_canvas.bind('<Configure>', lambda e: self._on_canvas_config(
            e, self._mb_canvas
        ))
        self._mb_canvas.create_window(
            (0, 0), window=self._second_mb_fr, anchor="nw", tags="frame")
        self._mb_canvas.bind_all(
            "<MouseWheel>", lambda: self._on_mousewheel(self._mb_canvas))
        self.mb_entries = []
        mb_var_list = []
        self._check_mb = []
        total_rows = len(tablist)
        total_columns = len(tablist[0])
        for i in range(total_rows):
            newrow = []
            for j in range(total_columns):
                self._mbe = Entry(self._second_mb_fr, fg=BLACK, bg=IVORY,
                                    font=FONT_L)
                self._mbe.grid(row=i, column=j, sticky=EW)
                self._second_mb_fr.columnconfigure(j, weight=1)
                self._mbe.insert(END, tablist[i][j])
                newrow.append(self._mbe)
                self._mbe.configure(
                    state='disabled', disabledbackground="white",
                    disabledforeground="black")
                if j == 1 and i != 0:
                    reg_hover = Hovertip(
                        self._mbe, self._get_desc(str(self._mbe.get())))
            self.mb_entries.append(newrow)
            if i != 0:
                self._check_mb.append(IntVar())
                mb_var_list.append(self._check_mb[i-1])
                check_m = Checkbutton(self._second_mb_fr, variable=self._check_mb[i-1],
                                        onvalue=i, offvalue=0,
                                        command=lambda: self._check_clicked(self._modb_fr, "mb",
                                                                            mb_var_list))
                check_m.grid(row=i, column=j+1, padx=(0,15))

    def _load_measurements(self, window, idy, tablist):
        self._main_frame = Frame(window, borderwidth=8,
                                 relief="ridge", bg="green")
        window.columnconfigure([0,1,2,3,4,5,6,7,8], weight=1)
        window.rowconfigure([11,12,13,14,15,16,17], weight=1)
        self._my_canvas = Canvas(
            self._main_frame, bg=IVORY)
        self._my_canvas.grid(column=0, row=0, sticky=NSEW)
        self._main_frame.grid(column=0, columnspan=5,
                    row=6, rowspan=9, sticky=NSEW, padx=(0, 50))
        self._second_frame = Frame(self._my_canvas)
        self._second_frame.pack(expand=True, fill='both')
        self._main_frame.columnconfigure(0, weight=1)
        self._main_frame.rowconfigure(0, weight=1)
        sb = Scrollbar(self._main_frame, orient='vertical',
                       command=self._my_canvas.yview)
        sb.grid(column=0, row=0, sticky="NSE")
        self._my_canvas.configure(yscrollcommand=sb.set)
        self._my_canvas.bind('<Configure>', lambda e: self._on_canvas_config(
            e, self._my_canvas
        ))
        self._my_canvas.create_window(
            (0, 0), window=self._second_frame, anchor="nw", tags="frame")
        self._my_canvas.bind_all(
            "<MouseWheel>", lambda: self._on_mousewheel(self._my_canvas))

        if tablist:
            total_rows = len(tablist)
            total_columns = len(tablist[0])
        else:
            tablist = [('Measurement', 'Uplink (min)',
                        'Interval in Mins', 'Sample Count', 'Value')]
            total_rows = 1
            total_columns = 5
        self._entries = []
        meas_var_list = []
        self._check_meas = []

        for i in range(total_rows):
            newrow = []
            for j in range(total_columns):
                if idy == 'mb':
                    self._e = Entry(self._second_frame, fg=BLACK, bg=IVORY,
                                    font=FONT_L)
                else:
                    self._e = Entry(self._second_frame, bg=IVORY, fg=BLACK,
                                    font=FONT_L)
                self._e.grid(row=i, column=j, sticky=EW)
                self._second_frame.columnconfigure(j, weight=1)
                self._e.insert(END, tablist[i][j])
                newrow.append(self._e)
                if j != 4:
                    self._e.configure(validate="key", validatecommand=(
                        window.register(self._handle_input), '%P', '%d'))
                if i == 0 or j == 0 or j == 4:
                    self._e.configure(
                        state='disabled', disabledbackground="white",
                        disabledforeground="black")
                    if j == 0 and i != 0:
                        reg_hover = Hovertip(
                            self._e, self._get_desc(str(self._e.get())))
                else:
                    self._e.bind(
                        "<FocusOut>", lambda e: self._change_sample_interval(e))
                if self._e.get() == 'TMP2' or self._e.get() == 'CNT1' or self._e.get() == 'CNT2':
                    self._bind_io = self._e.bind(
                        "<Button-1>", lambda e: self._open_ios_w(e.widget.get()))
                    self._e.configure(disabledforeground="green")
                    self._change_on_hover(self._e)
                    self._e.configure(cursor='hand2')
                elif self._e.get() == 'CC1' or self._e.get() == 'CC2' or self._e.get() == 'CC3':
                    self._bind_cc = self._e.bind(
                        "<Button-1>", lambda e: self._cal_cc(e.widget.get()))
                    self._e.configure(disabledforeground="green")
                    self._change_on_hover(self._e)
                    self._e.configure(cursor='hand2')
                elif self._e.get() in self.ftma_specs:
                    self._bind_ft = self._e.bind(
                        "<Button-1>", lambda e: self.open_ftma(e.widget.get()))
                    self._e.configure(disabledforeground="green")
                    self._e.configure(cursor='hand2')
                    self._change_on_hover(self._e)

            self._entries.append(newrow)
            if i != 0:
                self._check_meas.append(IntVar())
                meas_var_list.append(self._check_meas[i-1])
                check_meas = Checkbutton(self._second_frame, variable=self._check_meas[i-1],
                                            onvalue=i, offvalue=0,
                                            command=lambda: self._check_clicked(window, idy,
                                                                                meas_var_list))
                check_meas.grid(row=i, column=j+1, padx=(0,15))


    def _remove_mb_reg(self, idy, check):
        ticked = []
        self.dev_to_remove = ""
        row = None
        if check:
            for cb in check:
                if cb.get():
                    row = cb.get()
                    ticked.append(row)
            for tick in ticked:
                for i in range(len(self.mb_entries)):
                    if i == tick:
                            log_func(
                                "User attempting to remove modbus register..")
                            device = self.mb_entries[i][0]
                            self.dev_to_remove = device.get()
                            mb_reg_to_change = self.mb_entries[i][1]
                            mb_reg_to_change = mb_reg_to_change.get()
                            self.binding_interface.modbus_reg_del(mb_reg_to_change)
            self.binding_interface.get_modbus(self._on_remove_mb_reg)
            self.binding_interface.get_measurements(self._on_get_measurements_done_cb)

    def _set_meas_to_zero(self, check):
        ticked = []
        row = None
        if check:
            for cb in check:
                if cb.get():
                    row = cb.get()
                    ticked.append(row)
            for tick in ticked:
                for i in range(len(self._entries)):
                    if i == tick:
                        log_func(
                            "User attempting to set measurement interval 0..")
                        meas_chang = self._entries[i][0]
                        meas_chang = meas_chang.get()
                        inv_chang = self._entries[i][2]
                        uplink_chang = self._entries[i][1]
                        self.binding_interface.change_interval(meas_chang, 0)
                        inv_chang.delete(0, END)
                        inv_chang.insert(0, 0)
                        uplink_chang.delete(0, END)
                        uplink_chang.insert(0, 0)
                    for cb in check:
                        if cb.get() == tick:
                            cb.set(0)

    def _on_get_last_val(self, resp):
        meas = resp[1].split(":")[0]
        try:
            self.val = resp[1].split(":")[1].replace(" ", "")
        except IndexError:
            log_func("Failed to get measurement reading.")
            self.val = "Failed"
        for i in self._entries:
            v = i[0].get()
            if v == meas:
                val_col = i[4]
                val_col.configure(state='normal')
                val_col.insert(0, self.val)
                val_col.configure(state='normal')

    def _insert_last_value(self, check):
        ticked = []
        row = None
        if check:
            for cb in check:
                if cb.get():
                    row = cb.get()
                    ticked.append(row)
            for tick in ticked:
                for i in range(len(self._entries)):
                    if i == tick:
                        log_func(
                            "Retrieving last value for each ticked measurement.")
                        meas = self._entries[i][0]
                        meas = meas.get()
                        val_col = self._entries[i][4]
                        val_col.delete(0, END)
                        self.binding_interface.get_last_value(meas, self._on_get_last_val)
                    for cb in check:
                        if cb.get() == tick:
                            cb.set(0)

    def _check_clicked(self, window, idy, check):
        for cb in check:
            if cb.get():
                log_func(f"Checkbutton clicked : {cb.get()}")
                if idy == 'mb':
                    self._del_reg = Button(window, text='Remove',
                                           bg=IVORY, fg=BLACK, font=FONT,
                                           activebackground="green", activeforeground=IVORY,
                                           command=lambda: self._remove_mb_reg(idy, check))
                    self._del_reg.grid(column=4, row=8, sticky=NE)
                else:
                    self._rm_int = Button(window, text='Set Interval 0',
                                          bg=IVORY, fg=BLACK, font=FONT,
                                          activebackground="green", activeforeground=IVORY,
                                          command=lambda: self._set_meas_to_zero(check))
                    self._rm_int.grid(column=0, row=15, rowspan=2, sticky=W)

                    self._last_val_btn = Button(window, text='Get Value',
                                          bg=IVORY, fg=BLACK, font=FONT,
                                          activebackground="green", activeforeground=IVORY,
                                          command=lambda: self._insert_last_value(check))
                    self._last_val_btn.grid(column=1, row=15, rowspan=2, sticky=W)

    def _change_on_hover(self, entry):
        e = entry.get()
        reg_hover = Hovertip(self._e, self._get_desc(entry.get()))

    def _on_get_mp_done_cb(self, resp):
        self.midpoint = resp[1][0].split()[1]
        self._fill_cc_term()
        return self.midpoint

    def _cal_cc(self, cc):
        self._cc_window = Toplevel(self.master)
        self._cc_window.title(f"Current Clamp Calibration {cc}")
        self._cc_window.geometry("650x600")
        self._cc_window.configure(bg=IVORY)
        log_func("User opened window to calibrate current clamp..")
        self.binding_interface.get_midpoint(cc, self._on_get_mp_done_cb)
        self.selected_cc = cc

        cc_val_lab = Label(self._cc_window, text="Set current clamp values",
            bg=IVORY, font=FONT)
        cc_val_lab.grid(column=1, row=1, pady=10, columnspan=2)
        on_hover = Hovertip(
            cc_val_lab, "Use this page to scale the current clamp")

        self._outer_cc = Combobox(self._cc_window)
        self._outer_cc.grid(column=0, row=2)
        on_hover = Hovertip(
            self._outer_cc, "Set outer and inner current (Amps/millivolts).")
        self._outer_cc['values'] = ('100, 50',
                                    '200, 33'
                                    )

        outer_cc_lab = Label(self._cc_window, text="Amps/millivolts",
            bg=IVORY, font=FONT)
        outer_cc_lab.grid(column=1, row=2)
        on_hover = Hovertip(
            outer_cc_lab, "Set outer and inner current.")

        send_btn = Button(self._cc_window,
                          text="Send", command=lambda: self._send_cal(cc),
                          bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        send_btn.grid(column=2, row=2)

        self._cal_terminal = Text(self._cc_window,
                                  height=20, width=80,
                                  bg=BLACK, fg=LIME_GRN)
        self._cal_terminal.grid(column=0, row=4, columnspan=5)
        self._cal_terminal.configure(state='disabled')

        cal_btn = Button(self._cc_window,
                        text="Calibrate ADC",
                        command=lambda:self._calibrate(),
                        bg=IVORY, fg=BLACK, font=FONT,
                        activebackground="green", activeforeground=IVORY)
        cal_btn.grid(column=0, row=5, pady=10)
        on_hover = Hovertip(
            cal_btn, "Calibrate current clamp midpoint, make sure there is no live current.")

        mp_entry = Entry(self._cc_window, bg=IVORY)
        mp_entry.grid(column=3, row=5)
        on_hover = Hovertip(
            mp_entry, "Manually calibrate ADC midpoint. (Default 2048)")

        mp_btn = Button(self._cc_window, text="Set midpoint",
                        bg=IVORY, fg=BLACK, font=FONT,
                        activebackground="green", activeforeground=IVORY,
                        command=lambda: self._set_midpoint(mp_entry, cc))
        mp_btn.grid(column=4, row=5)
        on_hover = Hovertip(
            mp_btn, "Manually calibrate ADC midpoint. (Default 2048)")

        self._cal_label = Label(self._cc_window, text="",
        bg=IVORY)
        self._cal_label.grid(column=3, row=6)

        help_cc = Label(self._cc_window, text="Help")
        help_cc.grid(column=4, row=7)
        help_hover = Hovertip(help_cc, '''Midpoint
            Use this page to calibrate each phase of current.\t\n
            Here you can set Exterior and Interior values which are defaulted to 100A and 50mV respectively.\t\n
            You must set the midpoint when first configuring each phase, achieve this by simply selecting calibrate ADC.\t\n
            There is also the option to set the midpoint manually. A typical value is around 2030-2048.\t\n
            ''')

    def _fill_cc_term(self):
        self._cal_terminal.configure(state='normal')
        self._cal_terminal.delete('1.0', END)
        if self.cc_gain:
            for i in self.cc_gain:
                self._cal_terminal.insert(INSERT, i + "\n")
            self._cal_terminal.insert(INSERT, "\nMidpoint: " + self.midpoint + "\n")
            self._cal_terminal.configure(state='disabled')

    def _set_midpoint(self, entry, cc):
        log_func("User attempting to set current clamp midpoint..")
        v = entry.get()
        try:
            v = int(v)
        except ValueError:
            self._cal_label['text'] = "You must enter an integer"
        else:
            mp = entry.get()
            self.binding_interface.update_midpoint(mp, cc, self._on_update_mp_done_cb)
            self._cal_label['text'] = ""

    def _on_update_mp_done_cb(self, resp):
        self.binding_interface.get_midpoint(self.selected_cc, self._on_get_mp_done_cb)

    def _calibrate(self):
        log_func("User attempting to calibrate ADC automatically..")
        dev_off = tkinter.messagebox.askyesno(
            "Calibrate", "Has the current been switched off?", parent=self._cc_window)
        if dev_off:
            self.binding_interface.current_clamp_calibrate(self._on_update_mp_done_cb)
        else:
            tkinter.messagebox.showinfo(
                "", "Turn off live current.", parent=self._cc_window)

    def _send_cal(self, cc):
        log_func("User attempting to manually calibrate outer and inner cc...")
        if self._outer_cc.get():
            vals = self._outer_cc.get().split(', ')
            outer = int(vals[0])
            inner = int(vals[1])
            cc = cc[2]
            self._cal_terminal.configure(state='normal')
            self._cal_terminal.delete('1.0', END)
            self.binding_interface.set_outer_inner_cc(cc, outer, inner, self._on_update_cc_gain_cb)
        else:
            tkinter.messagebox.showerror(
                "Error", "Fill in missing fields.", parent=self._cc_window)

    def _enter_cmd(self, cmd):
        self._terminal.configure(state='normal')
        self._terminal.delete('1.0', END)
        cmd_text = cmd.get()
        self.binding_interface.do_cmd_multi(cmd_text, self._on_get_cmd_multi_done_cb)
        cmd.delete(0, END)

    def open_ftma(self, meas):
        self.binding_interface.get_spec_ftma_coeffs(str(meas), self._on_get_ftma_coeffs)

    def _on_get_ftma_coeffs(self, resp):
        self.coeffs = []
        d = resp[1].split(': ')
        meas = d[0].split("'")[1]
        for i in d[1:]:
            v = re.findall(r'(\d+\.\d+)', i)
            self.coeffs.append(v[0])
        self._cal_ftma(meas)

    def _cal_ftma(self, meas):
        self._ftma_window = Toplevel(self.master)
        self._ftma_window.title(f"4-20mA Calibration ({meas})")
        self._ftma_window.geometry("375x485")
        self._ftma_window.configure(bg=IVORY)

        name_title = Label(self._ftma_window, text="Change measurement name",
            bg=IVORY, font=FONT)
        name_title.grid(column=0, row=0, columnspan=2)

        ftma_lab = Label(self._ftma_window, text="Set coefficients",
            bg=IVORY, font=FONT)
        ftma_lab.grid(column=0, row=2, columnspan=2)

        a_lab = Label(self._ftma_window, text="A (Default 0):",
            bg=IVORY, font=FONT)
        a_lab.grid(column=0, row=3, sticky=E)

        b_lab = Label(self._ftma_window, text="B (Default 1):",
            bg=IVORY, font=FONT)
        b_lab.grid(column=0, row=4, sticky=E)

        c_lab = Label(self._ftma_window, text="C (Default 0):",
            bg=IVORY, font=FONT)
        c_lab.grid(column=0, row=5, sticky=E)

        d_lab = Label(self._ftma_window, text="D (Default 0):",
            bg=IVORY, font=FONT)
        d_lab.grid(column=0, row=6, sticky=E)

        name_entry = Entry(self._ftma_window,
                        bg=IVORY, fg=CHARCOAL,
                        font=('Arial', 14, 'bold'))
        name_entry.grid(column=0, row=1, columnspan=2)

        a_entry = Entry(self._ftma_window,
                        bg=IVORY, fg=CHARCOAL,
                        font=('Arial', 14, 'bold'))
        a_entry.grid(column=1, row=3, sticky=W)

        b_entry = Entry(self._ftma_window,
                        bg=IVORY, fg=CHARCOAL,
                        font=('Arial', 14, 'bold'))
        b_entry.grid(column=1, row=4, sticky=W)

        c_entry = Entry(self._ftma_window,
                        bg=IVORY, fg=CHARCOAL,
                        font=('Arial', 14, 'bold'))
        c_entry.grid(column=1, row=5, sticky=W)

        d_entry = Entry(self._ftma_window,
                        bg=IVORY, fg=CHARCOAL,
                        font=('Arial', 14, 'bold'))
        d_entry.grid(column=1, row=6, sticky=W)

        ftma_btn = Button(self._ftma_window, text="Commit",
                                   command=lambda : self._send_coeffs(
                                    str(name_entry.get()),
                                    [a_entry.get(), b_entry.get(), c_entry.get(), d_entry.get()],
                                    meas),
                                   bg=IVORY, fg=BLACK, font=FONT,
                                   width=20, activebackground="green",
                                   activeforeground=IVORY)
        ftma_btn.grid(column=0, row=7, columnspan=2)

        graph_canv = Canvas(self._ftma_window)
        graph_canv.grid(column=0, row=8, columnspan=2)


        X_START = 70
        X_END = 340
        Y_START = 220
        Y_END = 50
        graph_canv.create_line(X_START, Y_START, X_END, Y_START)  # x axis
        graph_canv.create_line(X_START, Y_START, X_START, Y_END)  # y axis

        EQU_X_OFFSET = 110
        EQU_Y_OFFSET = -40

        X_AXIS_LAB_X_OFFSET = -60
        X_AXIS_LAB_Y_OFFSET = -110

        Y_AXIS_LAB_X_OFFSET = 125
        Y_AXIS_LAB_Y_OFFSET = 30

        X_AXIS_NUM_Y_OFFSET = 10
        Y_AXIS_NUM_X_OFFSET = -10

        MA_MIN = 4
        MA_MAX = 20

        #title
        graph_canv.create_text(X_START + EQU_X_OFFSET, X_START + EQU_Y_OFFSET, text="y = A + Bx + Cx² + Dx³")
        #y axis label
        graph_canv.create_text(X_START + X_AXIS_LAB_X_OFFSET, Y_START + X_AXIS_LAB_Y_OFFSET, text="Output", angle=90)
        #x axis label
        graph_canv.create_text(X_START + Y_AXIS_LAB_X_OFFSET, Y_START + Y_AXIS_LAB_Y_OFFSET, text="mA")

        unit_min_y = self.calculate_output(MA_MIN)
        unit_max_y = self.calculate_output(MA_MAX)

        pixel_y_range = Y_START - Y_END
        unit_y_range = unit_max_y - unit_min_y

        unit_to_pixel_y = pixel_y_range / unit_y_range

        pixel_x_range = X_END - X_START
        ma_x_range = MA_MAX - MA_MIN

        unit_to_pixel_x = pixel_x_range / ma_x_range

        PNT_SIZE=3
        # Draw the X axis labels
        for mA in range(MA_MIN, MA_MAX + 1, 4):
            x = X_START + (mA - MA_MIN) * unit_to_pixel_x
            graph_canv.create_text(x, Y_START + X_AXIS_NUM_Y_OFFSET, text=str(mA))
            graph_canv.create_line(x, Y_START, x, Y_END)  # y line

        Y_STEPS = 10
        OUTPUT_STEP_SCALE = unit_y_range / Y_STEPS

        y_labels = []
        t_max_width = 0

        # Draw the Y axis labels
        for i in range(0, Y_STEPS + 1):
            output = unit_min_y + i * OUTPUT_STEP_SCALE
            y = Y_START - ((output - unit_min_y) * unit_to_pixel_y)
            graph_canv.create_line(X_START, y, X_END, y)  # x line
            text = "%G" % output
            t = graph_canv.create_text(0, 0, text=text)
            t_size = graph_canv.bbox(t)
            t_width = t_size[2] - t_size[0]
            if t_width > t_max_width:
                t_max_width = t_width
            y_labels += [(t, y)]

        # Reposition Y labels
        for t, y in y_labels:
            graph_canv.move(t, X_START - t_max_width / 2, y) # Half because it's centre, not corner drawn

        # Draw the data points
        for mA in range(MA_MIN, MA_MAX + 1):
            output = self.calculate_output(mA)
            x = X_START + ((mA - MA_MIN) * unit_to_pixel_x)
            y = Y_START - ((output - unit_min_y) * unit_to_pixel_y)
            graph_canv.create_oval(x - PNT_SIZE, y - PNT_SIZE, x + PNT_SIZE, y + PNT_SIZE, fill='blue')

    def calculate_output(self, milliamps):
        output = float(self.coeffs[0]) + \
            float(self.coeffs[1]) * milliamps + \
            float(self.coeffs[2]) * milliamps ** 2 + \
            float(self.coeffs[3]) * milliamps ** 3
        return output

    def _send_coeffs(self, name, args, meas):
        find_name = re.findall("[a-zA-z0-9]+", name)
        if find_name:
            find_name = find_name[0]
        if len(name) > 4:
            tkinter.messagebox.showerror(
                "Error", "Maximum length of name is 4.",
                parent=self._ftma_window)
            return
        elif not find_name and name:
            tkinter.messagebox.showerror(
                "Error", "Invalid characters in name",
                parent=self._ftma_window
                )
            return
        elif len(find_name):
            self.binding_interface.set_ftma_name(meas, find_name, self.on_update_ftma_name_cb)
            meas = find_name
        if len(args[0]):
            try:
                a_val = float(args[0])
            except ValueError:
                a_val = args[0]
                log_func("Cannot convert coefficient A to a float.")
        else:
            a_val = 0.
        if len(args[1]):
            if args[1] == '0':
                b_val = 1.
            else:
                try:
                    b_val = float(args[1])
                except ValueError:
                    b_val = args[0]
                    log_func("Cannot convert coefficient B to a float.")
        else:
            b_val = 1.
        if len(args[2]):
            try:
                c_val = float(args[2])
            except ValueError:
                c_val = args[0]
                log_func("Cannot convert coefficient C to a float.")
        else:
            c_val = 0.
        if len(args[3]):
            try:
                d_val = float(args[3])
            except ValueError:
                d_val = args[0]
                log_func("Cannot convert coefficient D to a float.")
        else:
            d_val = 0.
        if  isinstance(a_val, float) and \
            isinstance(b_val, float) and \
            isinstance(c_val, float) and \
            isinstance(d_val, float):
            self.binding_interface.set_coeffs(a_val, b_val, c_val, d_val, meas, self.on_update_coeffs_cb)
        else:
            tkinter.messagebox.showerror(
                "Error", "Values must be a valid integer.",
                parent=self._ftma_window,
                )

    def on_update_coeffs_cb(self, args):
        self._ftma_window.destroy()

    def on_update_ftma_name_cb(self, resp):
        self.binding_interface.get_measurements(self._on_get_measurements_done_cb)

    def _pop_lora_entry(self):
        if self.dev_eui:
            self._eui_entry.insert(0, self.dev_eui)
        if self.app_key:
            self._app_entry.insert(0, self.app_key)

    def _open_lora_config(self, frame):
        img = Image.open(R_LOGO)
        icon = ImageTk.PhotoImage(img)
        self._conn_fr.img_list.append(icon)
        lora_c_label = Label(frame, text="LoRaWAN configuration",
                             bg=IVORY, font=FONT_L)
        lora_c_label.grid(column=5, row=6, sticky="E", columnspan=2)

        lora_label = Label(frame, text="Dev EUI: ",
                           bg=IVORY, font=FONT)
        lora_label.grid(column=5, row=7, sticky="E")

        self._eui_entry = Entry(frame, font=FONT, bg=IVORY)
        self._eui_entry.grid(column=6, row=7, sticky="NSEW")

        add_eui_btn = Button(frame, image=icon,
                             command=self._insert_eui, bg=IVORY, fg=BLACK, font=FONT,
                             activebackground="green", activeforeground=IVORY)
        add_eui_btn.grid(column=7, row=7, sticky="NSEW")

        app_label = Label(frame, text="App key: ",
                          bg=IVORY, font=FONT)
        app_label.grid(column=5, row=8, sticky="E")

        self._app_entry = Entry(frame, font=FONT, bg=IVORY)
        self._app_entry.grid(column=6, row=8, sticky="NSEW")

        add_app_btn = Button(frame, image=icon,
                             command=self._insert_appkey,
                             bg=IVORY, fg=BLACK, font=FONT,
                             activebackground="green", activeforeground=IVORY)
        add_app_btn.grid(column=7, row=8, sticky="NSEW")

        send_btn = Button(frame, text="Send",
                          command=self._save_cmd, bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        send_btn.grid(column=7, row=9, sticky="NSEW")

        save_btn = Button(frame, text="Save",
                          command=self.binding_interface.save, bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        save_btn.grid(column=7, row=10, sticky="NSEW")

        lora_l = Label(frame, text="Comms: ",
                       bg=IVORY, font=FONT)
        lora_l.grid(column=5, row=9, sticky="E")
        lora_hover = Hovertip(
                lora_l, "External Communications Status")

        self._lora_status = Label(frame, text="",
                                  bg=IVORY, font=FONT)
        self._lora_status.grid(column=6, row=9, sticky=W)

        self._lora_confirm = Label(frame, text="",
                                   font=FONT, bg=IVORY)
        self._lora_confirm.grid(column=6, row=9, sticky="E")

    def _save_cmd(self):
        log_func("User attempting to save new lora configuration..")
        dev_eui = self._eui_entry.get()
        app_key = self._app_entry.get()
        if len(dev_eui) == 16 and len(app_key) == 32:
            self.binding_interface.set_dev_eui(dev_eui, self._on_get_dev_eui_done_cb)
            self.binding_interface.set_app_key(app_key, self._on_get_app_key_done_cb)
            self._lora_confirm.configure(text="Configuration sent.")
        else:
            self._lora_confirm.configure(
                text="Missing fields or bad character limit.")

    def _insert_appkey(self):
        self.binding_interface.gen_rand_app(self._on_update_app_key)

    def _on_update_app_key(self, resp):
        app_key = resp[1]
        self._app_entry.delete(0, END)
        self._app_entry.insert(0, app_key)

    def _insert_eui(self):
        self.binding_interface.gen_rand_eui(self._on_update_dev_eui)

    def _on_update_dev_eui(self, resp):
        dev_eui = resp[1]
        self._eui_entry.delete(0, END)
        self._eui_entry.insert(0, dev_eui)

    def _close_application(self):
        self.binding_interface.finish()
        root.destroy()

    def _on_closing(self):
        self._close_application()

    def _resize_image(self, e, label, img, frame):
        img_height = img.height
        img_width = img.width
        new_width  = e.width
        new_height = e.height

        width_ratio = new_width / img_width
        height_ratio = new_height / img_height

        if width_ratio < height_ratio:
            new_height = int(width_ratio * img_height)
        else:
            new_width = int(height_ratio * img_width)
        if new_height > 0 and new_width > 0:
            image = img.resize((new_width, new_height))
            new_image = ImageTk.PhotoImage(image)
            label.configure(image=new_image)
            frame.img_list.append(new_image)

    def main_modbus_w(self):
        name = Label(self._modb_fr, text="Modbus device templates",
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

        unit_id = Label(self._modb_fr, text="Modbus registers on Template",
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
            self._modb_fr, text="Current modbus settings on OSM", font=FONT, bg=IVORY)
        self._current_mb.grid(column=3, row=0, columnspan=3, sticky=NSEW)
        self.template_list.curselection()
        with self.db.conn:
            data = self.db.cur.execute(*modbus_db.GET_TMP_N)
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
                self._close_application()
            elif yesno == False:
                self._close_application()
        else:
            self._close_application()

    def _send_to_save(self):
        log_func("User attempting to save a new template")
        if self._modb_funcs.save_template():
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
        chosen_del_temp = self.db.cur.execute(*modbus_db.GET_TEMP_ID(chosen_template))
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
                        doc_temp_name = doc[i]['templates'][0]['name']
                        if doc_temp_name == edited_template:
                            templ = doc[i]['templates'][0]
                            template_name = templ['name']
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
        # This might not catch the right error, but catching all is bad.
        except AttributeError:
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
                    if doc[v]['templates'][0]['name'] == temp:
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
                    if doc[v]['templates'][0]['name'] == template:
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

    def _clear_template_reg_list(self, **kwargs):
        copy = kwargs["copy"]
        edited_tmp = kwargs["edited_tmp"]
        devices_dict = [{'templates_to_del': {'template': []}}]
        edited_tmp_id = kwargs["edited_tmp_id"]
        index = kwargs["index"]
        if copy == 'edit':
            self._modb_funcs.save_edit_yaml(
                copy, edited_tmp, devices_dict, edited_tmp_id)
            self.template_list.delete(index)
        with open(PATH + '/yaml_files/modbus_data.yaml', 'r') as f:
            doc = yaml.full_load(f)
            if doc:
                for y, item in enumerate(doc):
                    if item['templates'][0]['name'] == str(self.name_entry.get()):
                        del doc[y]
                        if len(doc) == 0:
                            del doc
                        with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                            if f:
                                yaml.dump(doc, f)

    def _save_edit(self, copy, window):
        log_func("User attempting to save template..")
        uid_list = []
        dev_list = []
        uids = self.db.cur.execute(*modbus_db.GET_UNIT_IDS)
        for uid in uids:
            uid_list.append(uid)
            dev_list.append(uid[1])
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
                    index = self.edited_selection[0]
                    edited_tmp = self.template_list.get(index)
                    get_tmp_id = None
                    edited_tmp_id = None
                    get_tmp_id = self.db.cur.execute(
                        *modbus_db.GET_TEMP_ID(edited_tmp))
                    fetched = get_tmp_id.fetchone()
                    if fetched:
                        edited_tmp_id = fetched[0]
                if copy == 'edit' and len(exists) == 0:
                    self._clear_template_reg_list(copy=copy, edited_tmp=edited_tmp, edited_tmp_id=edited_tmp_id, index=index)
                    self._send_to_yaml(window)
                elif copy == 'edit' and str(self.dev_entry.get()) == exists[0][1]:
                    self._clear_template_reg_list(copy=copy, edited_tmp=edited_tmp, edited_tmp_id=edited_tmp_id, index=index)
                    self._send_to_yaml(window)
                elif str(self.dev_entry.get()) in dev_list and copy != 'edit':
                    tkinter.messagebox.showerror(
                        "Error", "That device name is already taken.", parent=window)
                elif int(self.unit_spinbox.get()) not in [uid for uid, devn in exists] and copy != 'edit':
                    self._clear_template_reg_list(copy=copy, edited_tmp=edited_tmp, edited_tmp_id=edited_tmp_id, index=index)
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
            if isinstance(child, Entry) or isinstance(child, Combobox):
                if not child.get():
                    continue
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
                if self._modbus:
                    for dev in self._modbus:
                        if dev[0] not in curr_devs:
                            curr_devs += [ dev[0] ]
                regs = self.db.get_modbus_template_regs(chosen_template)
                if regs:
                    curr_regs = []
                    for reg in regs:
                        unit_id, bytes_fmt, hex_addr, func_type, \
                            data_type, reg_name, dev_name = reg
                        if unit_id not in curr_devs:
                            self.binding_interface.add_modbus_dev(unit_id,
                                                     dev_name,
                                                     "MSB" in bytes_fmt,
                                                     "MSW" in bytes_fmt)
                            curr_devs.append(unit_id)
                        if reg_name not in curr_regs:
                            self.binding_interface.add_modbus_reg(unit_id,
                                                    reg_name,
                                                    int(hex_addr, 16),
                                                    func_type,
                                                    data_type)
                    self.binding_interface.get_modbus(self._on_get_modbus_done_cb)
                    self.binding_interface.get_measurements(self._on_get_measurements_done_cb)
                else:
                    tkinter.messagebox.showerror(
                        "Error", "Device must be saved.", parent=root)
        else:
            tkinter.messagebox.showerror(
                "Error", "Select a template to write to device.", parent=root)


if __name__ == '__main__':
    own_dir = os.path.dirname(__file__)
    os.chdir(own_dir)
    root = config_gui_window_t()
    tag = os.popen("git describe --tags").read().split('-')[0]
    width, height = root.winfo_screenwidth(), root.winfo_screenheight()
    root.geometry('%dx%d+0+0' % (width, height))
    root.title(f"Open Smart Monitor Configuration: {tag}")
    root.resizable(True, True)
    root.configure(bg="lightgrey")
    root.protocol("WM_DELETE_WINDOW", root._on_closing)
    logging.basicConfig(
        format='[%(asctime)s.%(msecs)06d] GUI : %(message)s',
        level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S')
    binding.set_debug_print(binding.default_print)
    root.mainloop()
