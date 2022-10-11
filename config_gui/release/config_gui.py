#!/usr/bin/python3

import os
import sys
from tkinter import *
import tkinter.messagebox
from tkinter.ttk import Combobox, Notebook, Progressbar, Style
import webbrowser
import serial.tools.list_ports
import serial
import re
import binding
import yaml
from idlelib.tooltip import Hovertip
import subprocess
from tkinter.filedialog import askopenfilename
import threading
import traceback
from modbus_funcs import modbus_funcs_t
import modbus_db
from modbus_db import modb_database_t
from PIL import ImageTk, Image
import logging
import platform
import signal
from stat import *

MB_DB = modbus_db
FW_PROCESS = False
THREAD = threading.Thread
GET_REG_DESC = MB_DB.GET_REG_DESC

CMDS = ["ios ------ Print all IOs.",
        "io ------ Get/set IO set.",
        "sio ------ Enable Special IO.",
        "count ------ Counts of controls.",
        "version ------ Print version.",
        "comms ------ Send lora message",
        "comms_config ------ Set lora config",
        "interval ------ Set the interval",
        "samplecount ------ Set the samplecount",
        "debug ------ Set hex debug mask",
        "hpm ------ Enable/Disable HPM",
        "mb_setup ------ Change Modbus comms",
        "mb_dev_add ------ Add modbus dev",
        "mb_reg_add ------ Add modbus reg",
        "mb_get_reg ------ Get modbus reg",
        "mb_reg_del ------ Delete modbus reg",
        "mb_dev_del ------ Delete modbus dev",
        "mb_log ------ Show modbus setup",
        "save ------ Save config",
        "measurements ------ Print measurements",
        "fw+ ------ Add chunk of new fw.",
        "fw@ ------ Finishing crc of new fw.",
        "reset ------ Reset device.",
        "cc ------ CC value",
        "cc_cal ------ Calibrate the cc",
        "w1 ------ Get temperature with w1",
        "timer ------ Test usecs timer",
        "temp ------ Get the temperature",
        "humi ------ Get the humidity",
        "dew ------ Get the dew temperature",
        "comms_conn ------ LoRa connected",
        "wipe ------ Factory Reset",
        "interval_min ------ Get/Set interval minutes",
        "bat ------ Get battery level.",
        "pulsecount ------ Show pulsecount.",
        "lw_dbg ------ LoraWAN Chip Debug",
        "light ------ Get the light in lux.",
        "sound ------ Get the sound in lux.",
        "no_lw ------ Don't need LW for measurements",
        "sleep ------ Sleep",
        "power_mode ------ Power moded setting",
        "can_impl ------ Send example CAN message",
        "debug_mode ------ Set/unset debug mode"
        ]

STD_MEASUREMENTS_DESCS = {
    'FW': 'Firmware Version',
    'PM10': 'Air Quality (Particles)',
    'PM25': 'Air Quality (Particles)',
    'CC1': 'Current Clamp Phase 1',
    'CC2': 'Current Clamp Phase 2',
    'CC3': 'Current Clamp Phase 3',
    'TMP2': 'One Wire Temperature',
    'TMP3': 'n/a',
    'TEMP': 'Temperature',
    'HUMI': 'Humidity',
    'BAT': 'Battery',
    'CNT1': 'Pulsecount',
    'CNT2': 'Pulsecount',
    'LGHT': 'Light',
    'SND': 'Sound'
}

PATH = os.path.dirname(__file__)
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
GET_TMP_N = MB_DB.GET_TMP_N
GET_TEMP_ID = MB_DB.GET_TEMP_ID
GET_UNIT_IDS = MB_DB.GET_UNIT_IDS
ICONS_T = PATH + "/osm_pictures/logos/icons-together.png"
DVT_IMG = PATH + "/osm_pictures/logos/OSM+Powered.png"
OSM_1 =   PATH + "/osm_pictures/logos/lora-osm.png"
R_LOGO =  PATH + "/osm_pictures/logos/shuffle.png"
GRPH_BG = PATH + "/osm_pictures/logos/graph.png"
PARAMS =  PATH + "/osm_pictures/logos/parameters.png"
OPEN_S =  PATH + "/osm_pictures/logos/opensource-nb.png"
LEAF =    PATH + "/osm_pictures/logos/leaf2.png"


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
        self.db = modb_database_t()
        self._modb_funcs = modbus_funcs_t()
        self._connected = False
        self._changes = False
        self._widg_del = False
        with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
            pass
        with open(PATH + '/yaml_files/del_file.yaml', 'w') as df:
            pass

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
        self._notebook.pack(fill='both', expand=True)

        self._conn_fr = Frame(self._notebook, width=1400, height=1000,
                              bg=IVORY, padx=50, pady=50)
        self._conn_fr.pack(fill='both', expand=True)
        self._conn_fr.pack_propagate(0)

        self._main_fr = Frame(self._notebook,
                              bg=IVORY, padx=50)
        self._main_fr.pack(fill='both', expand=True)
        self._main_fr.pack_propagate(0)

        self._adv_fr = Frame(self._notebook, bg=IVORY,
                             pady=50)
        self._adv_fr.pack(fill='both', expand=True)
        self._adv_fr.pack_propagate(0)

        self._modb_fr = Frame(self._notebook,
                              bg=IVORY, padx=50, pady=50)
        self._modb_fr.pack(fill='both', expand=True)
        self._modb_fr.pack_propagate(0)

        self._debug_fr = Frame(self._notebook, bg=IVORY)
        self._debug_fr.pack(fill='both', expand=True)
        self._debug_fr.pack_propagate(0)

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
        self._progress = Progressbar(
            self._conn_fr, orient=HORIZONTAL, length=100, mode='determinate', maximum=150)
        self._progress.pack()
        self._progress.pack_forget()
        self._fw_label = Label(self._conn_fr, text="", bg=IVORY)
        self._fw_label.pack()

        self._welcome_p = Label(self._conn_fr, text="Welcome!\n\nOpen Smart Monitor Configuration",
                                font=FONT_XL, pady=50, bg=IVORY)
        self._welcome_p.pack()

        img = Image.open(OSM_1)
        osm1_logo = ImageTk.PhotoImage(img)
        self._conn_fr.img_list = []
        self._conn_fr.img_list.append(osm1_logo)
        self._osm1_lab = Label(
            self._conn_fr, image=osm1_logo, bg=IVORY)
        self._osm1_lab.pack()
        self._hide_connect()

    def _hide_connect(self):
        mode = None
        dev_sel = self._dev_dropdown.get()
        if self._connected == False:
            if not dev_sel:
                self._connect_btn.configure(state='disabled')
                self._downl_fw.configure(state='disabled')
            else:
                try:
                    mode = os.stat(dev_sel).st_mode
                    if S_ISCHR(mode):
                        self._connect_btn.configure(state='normal')
                        self._downl_fw.configure(state='normal')
                except Exception as e:
                    log_func(e)
                    self._connect_btn.configure(state='disabled')
                    self._downl_fw.configure(state='disabled')
            self._conn_fr.after(500, self._hide_connect)

    def _on_connect(self):
        self._downl_fw.pack_forget()
        self.dev_sel = self._dev_dropdown.get()
        if self.dev_sel:
            log_func("User attempting to connect to device.. : " + self.dev_sel)
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
                self._dev = None
                self._debug_parse = None
                try:
                    serial_obj = serial.Serial(port=self.dev_sel,
                                               baudrate=115200,
                                               bytesize=serial.EIGHTBITS,
                                               parity=serial.PARITY_NONE,
                                               stopbits=serial.STOPBITS_ONE,
                                               timeout=0)
                    self._dev = binding.dev_t(serial_obj)
                    self._debug_parse = binding.dev_debug_t(serial_obj)
                except Exception as e:
                    traceback.print_exc()
                    self._dev = None
                    self._debug_parse = None
                    msg = "Failed to open : " + str(e)
                    log_func(msg)
                    tkinter.messagebox.showerror(msg)

                if self._dev:
                    self._connected = True
                    if self._widg_del == False:
                        self._notebook.add(self._conn_fr, text="Connect",)
                        self._notebook.add(self._main_fr, text="Home Page")
                        self._notebook.add(
                            self._adv_fr, text='Advanced Config')
                        self._notebook.add(
                            self._modb_fr, text="Modbus Config")
                        self._notebook.add(self._debug_fr, text="Debug Mode")
                    self._notebook.select(1)
                    self._sensor_name = Label(self._main_fr, text="",
                                              bg=IVORY)
                    self._sensor_name.grid(
                        column=4, row=0, columnspan=3, sticky='E')

                    self._fw_version_body = Label(self._main_fr, text="")
                    self._fw_version_body.grid(column=5, row=1, columnspan=2,
                                               pady=(0, 50), sticky='E')

                    self._load_meas_l = Label(
                        self._main_fr, text="Current Measurements on OSM",
                        font=FONT_L, bg=IVORY)
                    self._load_meas_l.grid(
                        column=1, row=5, columnspan=2, sticky=E)

                    self._main_fr.img_list = []
                    img = Image.open(DVT_IMG)
                    dev_logo = ImageTk.PhotoImage(img)
                    self._main_fr.img_list.append(dev_logo)
                    dev_lab = Label(self._main_fr, image=dev_logo, bg=IVORY)
                    dev_lab.grid(column=6, row=14, rowspan=5)

                    web_url = Label(
                        self._main_fr, text="Click here to visit our website!",
                        font=FONT_XL, bg=IVORY, fg=OSM_GRN)
                    web_url.grid(column=6, row=19, columnspan=2, padx=(0, 30))
                    web_url.bind(
                        "<Button-1>",
                        lambda e: open_url("https://www.devtank.co.uk"))
                    web_url.configure(cursor='hand2')

                    banner_fr = Canvas(self._main_fr, bg=IVORY)
                    banner_fr.grid(column=0, row=20, columnspan=7, pady=100)
                    img3 = Image.open(ICONS_T)
                    emc_img = ImageTk.PhotoImage(img3)
                    emc_lab = Label(banner_fr, image=emc_img, bg=IVORY)
                    emc_lab.grid(column=0, row=0, padx=20)
                    self._main_fr.img_list.append(emc_img)

                    img_gr = Image.open(GRPH_BG)
                    gra_logo = ImageTk.PhotoImage(img_gr)
                    self._adv_fr.img_list = []
                    self._adv_fr.img_list.append(gra_logo)
                    self._gra_lab = Label(
                        self._adv_fr, image=gra_logo, bg=IVORY)
                    self._gra_lab.place(x=0, y=0)

                    self._terminal = Text(
                        self._adv_fr, bg=BLACK, fg=LIME_GRN,
                        width=100, height=30, borderwidth=10, relief="sunken")
                    self._terminal.pack()
                    self._terminal.configure(state='disabled')

                    cmd = Entry(self._adv_fr, width=82, fg=CHARCOAL,
                                font=FONT_L)
                    cmd.pack()
                    cmd.insert(0, 'Enter a command and hit return to send.')
                    cmd.bind("<Return>", lambda e: self._enter_cmd(cmd))
                    cmd.bind("<Button-1>",
                             lambda event:  self._clear_box(event, cmd))

                    man_config_btn = Button(self._adv_fr, text="List of Commands",
                                            command=self._manual_config,
                                            bg=IVORY, fg=BLACK, font=FONT,
                                            activebackground="green", activeforeground=IVORY)
                    man_config_btn.pack()
                    self._open_lora_config(self._main_fr)

                    self.modbus_opened = False
                    self._dbg_open = False
                    self._notebook.bind("<<NotebookTabChanged>>",
                                        lambda e: self._tab_changed(e,
                                                                    self._main_fr,
                                                                    self._notebook))
                    root.update()
                    get_mins = self._get_interval_mins()
                    if get_mins:
                        self._load_headers(self._main_fr, "rif", True)
                        self._add_ver()
                        self._pop_lora_entry()
                        self._pop_sensor_name()
                    else:
                        self._load_measurements(self._main_fr, "rif", None)

    def _get_interval_mins(self):
        get_mins = self._dev.do_cmd_multi("interval_mins")
        if get_mins:
            self._interval_min = get_mins[0].split()[4]
            return self._interval_min

    def _pop_sensor_name(self):
        serial_num = self._dev.do_cmd_multi("serial_num")
        if serial_num:
            ser_op = serial_num[0].split()[2]
            self._sensor_name.configure(
                text="Serial Number - " + ser_op,
                bg=IVORY, font=FONT)

    def _add_ver(self):
        vers = self._dev.do_cmd_multi("version")
        if vers:
            version = vers[0].split('-')[1]
            self._fw_version_body.configure(
                text="Current Firmware Version - " + version,
                bg=IVORY, font=FONT)

    def _tab_changed(self, event, frame, notebook):
        slction = notebook.select()
        log_func(f"User changed to tab {slction}.")
        if slction == '.!notebook.!frame4' and self.modbus_opened == False:
            self.modbus_opened = True
            self._main_modbus_w()
        elif slction == '.!notebook.!frame5' and self._dbg_open == False:
            self._dbg_open = True
            self._thread_debug()
        elif slction == '.!notebook.!frame2' and self.modbus_opened == True:
            self.modbus_opened = False
            self._fw_label.configure(text="")
            self._load_headers(frame, "rif", True)

    def _clear_box(self, event, entry):
        entry.delete(0, END)
        return

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
                ["sudo", "./" + PATH + "/static_program.sh", selected], shell=False)
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
        terminal.configure(state='normal')
        terminal.delete('1.0', END)
        terminal.insert(INSERT, cmd)
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
            self._dev.do_cmd(
                f"samplecount {meas_chang} {widget_val}")
            samp_chang.delete(0, END)
            samp_chang.insert(0, widget_val)
        elif int(widget_id) % 2 == 0:
            self._dev.do_cmd(
                f"interval {meas_chang} {widget_val}")
            if meas_chang == 'TMP2' and widget_val != '0':
                self._dev.do_cmd("interval CNT1 0")
                self._update_meas_tab('CNT1')
            elif meas_chang == 'CNT1' and widget_val != '0':
                self._dev.do_cmd("interval TMP2 0")
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
        self._dev.do_cmd(f"interval {meas_chang} {min}")
        uplink_chang.delete(0, END)
        uplink_chang.insert(0, int(min))
        # if user changes interval in mins for pulsecount or one wire
        if meas_chang == 'CNT1' and widget_val != '0':
            self._dev.do_cmd("interval TMP2 0")
            self._update_meas_tab('TMP2')
        elif meas_chang == 'TMP2' and widget_val != '0':
            self._dev.do_cmd("interval CNT1 0")
            self._update_meas_tab('CNT1')

    def _round_to_multiple(self, number, multiple):
        return multiple * round(number / multiple)

    def _do_ios_cmd(self, cmd, meas):
        log_func(f"User attempting to {cmd} IO for {meas}")
        inst = ''
        desc = ''
        if meas == 'TMP2':
            inst = 'en_w1'
            desc = 'One Wire Temperature'
        elif meas == 'CNT1' or meas == 'CNT2':
            inst = 'en_pulse'
            desc = 'Pulsecount'
        if meas == 'CNT2':
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
        if cmd == 'enable':
            # check if user is trying to overwrite existing io
            lines = self._dev.do_cmd_multi('ios')
            found = []
            for line in lines:
                if meas == "CNT1" and "IO 04 : USED" in line:
                    found.append(line)
                elif meas == "TMP2" and "IO 04 : USED" in line:
                    found.append(line)
                elif meas == "CNT2" and "IO 05 : USED" in line:
                    found.append(line)
            if found:
                self._ios_label.configure(
                    text="That IO is in use.", bg=RED, fg=IVORY)
            else:
                # activate io
                if meas == 'TMP2':
                    self._dev.do_cmd(f"{inst} 4 N")
                    self._ios_label.configure(
                        text=f"{desc} enabled.", bg=LIME_GRN, fg=BLACK)
                elif meas == 'CNT1':
                    self._dev.do_cmd(f"{inst} 4 U")
                elif meas == 'CNT2':
                    self._dev.do_cmd(f"{inst} 5 {pullup}")
        elif cmd == 'disable':
            # disable io
            if meas == 'TMP2':
                self._dev.do_cmd("io 4 : I N")
            elif meas == 'CNT1':
                self._dev.do_cmd("io 4 : I N")
            elif meas == 'CNT2':
                self._dev.do_cmd("io 5 : I N")
            self._ios_label.configure(text="IO disabled.", bg=RED, fg=IVORY)
        # update terminal output
        io_out = self._dev.do_cmd_multi("ios")
        self._ios_terminal.configure(state='normal')
        self._ios_terminal.delete('1.0', END)
        for i in io_out:
            if "IO 04" in i:
                self._ios_terminal.insert(INSERT, i + "\n")
            elif "IO 05" in i:
                self._ios_terminal.insert(INSERT, i + "\n")
        self._ios_terminal.configure(state='disabled')

    def _open_ios_w(self, meas):
        self.ios_page = Toplevel(self.master)
        self.ios_page.title(f"Set IO {meas}")
        self.ios_page.geometry("405x500")
        self.ios_page.configure(bg=IVORY)
        log_func("User opened IO window..")
        if meas != 'TMP2':
            io_d_label = Label(
                self.ios_page,
                text="Set Pull Up or Pull Down")
            io_d_label.grid(column=1, row=2)
            self._up_d_non = Combobox(self.ios_page)
            self._up_d_non.grid(column=1, row=3)
            self._up_d_non['values'] = ('Up', 'Down', 'None')
            if meas == 'CNT1':
                self._up_d_non.set('Up')
                self._up_d_non.configure(state='disabled')
        en_check = Button(self.ios_page,
                          text="Enable",
                          command=lambda: self._do_ios_cmd('enable', meas),
                          bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        en_check.grid(column=0, row=4)

        dis_check = Button(self.ios_page,
                           text="Disable",
                           command=lambda: self._do_ios_cmd('disable', meas),
                           bg=IVORY, fg=BLACK, font=FONT,
                           activebackground="green", activeforeground=IVORY)
        dis_check.grid(column=2, row=4)

        self._ios_label = Label(self.ios_page, text="", bg=IVORY)
        self._ios_label.grid(column=1, row=5)

        self._ios_terminal = Text(self.ios_page,
                                  height=20, width=50,
                                  bg=BLACK, fg=LIME_GRN)
        self._ios_terminal.grid(column=0, row=6, columnspan=3)
        io_out = self._dev.do_cmd_multi("ios")
        if io_out:
            for i in io_out:
                if "IO 04" in i:
                    self._ios_terminal.insert(INSERT, i + "\n")
                elif "IO 05" in i:
                    self._ios_terminal.insert(INSERT, i + "\n")
            help_lab = Label(self.ios_page, text="Help")
            help_lab.grid(column=2, row=7)
            help_hover = Hovertip(help_lab, '''
            You must enable this measurement for it to report data.\t\n
            W1 = One Wire Temperature.(TMP2)\t\n
            PLSCNT = Pulsecount.(CNT1/2)\t\n
            IO 04 is reserved for TMP2 or CNT1.\t\n
            IO 05 is reserved for CNT2.\t
            ''')
            self._ios_terminal.configure(state='disabled')
        else:
            tkinter.messagebox.showerror(
                "Error", "Press OK to reopen window..")
            self.ios_page.destroy()
            self._open_ios_w(meas)

    def _thread_debug(self):
        self._open_debug_w(self._debug_fr)
        log_func(f"User switched to tab 'Debug Mode'.")
        T1 = THREAD(
            target=lambda: self._load_debug_meas(self._debug_fr)
        )
        T1.daemon = True
        T1.start()
        if T1.is_alive():
            log_func('_thread_debug : thread 1 running...')
        else:
            log_func("_thread_debug : thread 1 complete")

    def _open_debug_w(self, frame):
        l_img = Image.open(LEAF)
        leaf_logo = ImageTk.PhotoImage(l_img)
        self._debug_fr.img_list = []
        self._debug_fr.img_list.append(leaf_logo)
        self._leaf_lab = Label(
            self._debug_fr, image=leaf_logo, bg=IVORY)
        self._leaf_lab.grid(column=0, row=0, columnspan=3)

        debug_btn = Button(frame,
                           text="Activate/Disable Debug Mode",
                           command=lambda: self._dev.do_debug("debug_mode"),
                           bg=IVORY, fg=BLACK, font=FONT,
                           activebackground="green", activeforeground=IVORY)
        debug_btn.grid(column=3, row=1, padx=((200, 50)))

        self._dbg_terml = Text(frame,
                               height=30, width=80,
                               bg=BLACK, fg=LIME_GRN,
                               borderwidth=10, relief="sunken")
        self._dbg_terml.grid(column=2, row=2, columnspan=3,
                             padx=((200, 50)))
        self._dbg_terml.configure(state='disabled')

        self._debug_first_fr = Frame(frame, bg="green", borderwidth=8,
                                     relief="ridge")
        self._debug_first_fr.grid(column=6, row=1,
                                  rowspan=30)

        self._dbg_canv = Canvas(
            self._debug_first_fr)
        self._dbg_canv.grid(column=0, row=0)

    def _load_debug_meas(self, frame):
        hdrs = [('Measurement', 'Value')]
        mmnts = self._dev.measurements('measurements')
        meas = []
        self._dbg_sec_fr = Frame(self._dbg_canv)
        self._dbg_sec_fr.grid(column=0, row=0)
        if mmnts:
            for m in mmnts:
                fw = [m[0]]
                fw.insert(1, 0)
                meas.append(fw)
            for i in range(len(meas)):
                row = tuple(meas[i])
                hdrs.append(row)
            total_rows = len(hdrs)
            total_columns = len(hdrs[0])
            self._deb_entries = []
            for i in range(total_rows):
                newrow = []
                for j in range(total_columns):
                    debug_e = Entry(self._dbg_sec_fr, width=15,
                                    bg=IVORY, fg=CHARCOAL,
                                    font=('Arial', 14, 'bold'))
                    debug_e.grid(row=i, column=j)
                    debug_e.insert(END, hdrs[i][j])
                    debug_e.configure(state='disabled',
                                      disabledbackground=IVORY,
                                      disabledforeground=BLACK)
                    newrow.append(debug_e)
                self._deb_entries.append(newrow)
        self._dbg_canv.create_window(
            (0, 0), window=self._dbg_sec_fr, anchor="nw")
        self._dbg_canv.bind_all(
            "<MouseWheel>", lambda: self._on_mousewheel(self._dbg_canv))
        debug_sb = Scrollbar(
            self._debug_first_fr, orient='vertical', command=self._dbg_canv.yview)
        debug_sb.grid(column=0, row=0, sticky="NSE")

        self._dbg_canv.configure(yscrollcommand=debug_sb.set)
        self._dbg_canv.bind('<Configure>', lambda e: self._dbg_canv.configure(
            scrollregion=self._dbg_canv.bbox("all")))

        h = self._dbg_terml.winfo_height()
        w = self._dbg_sec_fr.winfo_width()
        self._dbg_canv.configure(height=h, width=w)
        self._reload_debug_lines()

    def _reload_debug_lines(self):
        deb_list = self._dev.deb_readlines()
        for d in deb_list:
            if d:
                res = self._debug_parse.parse_msg(d)
                if res:
                    dbg_meas = res[0]
                    dbg_val = res[1]
                    for i in self._deb_entries:
                        meas = i[0].get()
                        if meas == dbg_meas:
                            val_to_change = i[1]
                            val_to_change.configure(state='normal')
                            val_to_change.delete(0, END)
                            val_to_change.insert(0, int(dbg_val))
                            val_to_change.configure(
                                state='disabled')
                    self._dbg_terml.configure(state='normal')
                    self._dbg_terml.insert('1.0', d + "\n")
                    self._dbg_terml.configure(state='disabled')
        self._dbg_terml.after(1500, self._reload_debug_lines)

    def _on_mousewheel(self, event, canvas):
        canvas.yview_scroll(-1*(event.delta/120), "units")

    def _get_desc(self, reg):
        desc = STD_MEASUREMENTS_DESCS.get(reg, None)
        spec_m = ['CC1', 'CC2', 'CC3', 'TMP2', 'CNT1', 'CNT2']
        if reg in spec_m:
            self._e.configure(cursor='hand2')
        elif desc is None:
            descr = self.db.conn.execute(GET_REG_DESC(reg))
            desc = descr.fetchone()[0]
        return desc

    def _change_uplink(self, frame, widg):
        if int(widg.get()):
            uplink = int(widg.get())
            if uplink > 255:
                self._dev.do_cmd(f"interval_mins 255")
            elif uplink < 3:
                self._dev.do_cmd(f"interval_mins 3")
            else:
                self._dev.do_cmd(f"interval_mins {uplink}")
            self._load_headers(frame, "rif", False)
        else:
            tkinter.messagebox.showerror(
                "Error", "Uplink interval must be an integer")

    def _handle_input(self, in_str, acttyp):
        if acttyp == '1':
            if not in_str.isdigit():
                return False
        return True

    def _load_headers(self, window, idy, get_mins):
        log_func("Getting measurements")
        self._sens_meas = self._dev.measurements("measurements")
        log_func("Got measurements")
        if idy == "mb":
            tablist = [('Device', 'Name', 'Hex Address',
                        'Data Type', 'Function')]
            log_func("Getting modbus")
            mb = self._dev.get_modbus()
            log_func("Got modbus")
            devs = mb.devices
            if devs:
                mmnts = []
                for dev in devs:
                    for reg in dev.regs:
                        i = (str(reg).split(','))
                        i.insert(0, str(dev.name))
                        mmnts.append(i)
                for i in range(len(mmnts)):
                    hx = hex(int(mmnts[i][2]))
                    row = list(mmnts[i])
                    row[2] = hx
                    tablist.append(row)
        else:
            if get_mins == False:
                self._get_interval_mins()
            tablist = [('Measurement', 'Uplink (%smin)' % self._interval_min,
                        'Interval in Mins', 'Sample Count')]
            if self._sens_meas:
                self._sens_meas[:] = [tuple(i) for i in self._sens_meas]
                for i in range(len(self._sens_meas)):
                    row = list(self._sens_meas[i])
                    for n in range(len(row)):
                        entry = row[n]
                        pos = entry.find("x")
                        if pos != -1:
                            row[n] = entry[0:pos]
                    row.insert(2, (int(self._interval_min) * int(row[1])))
                    tablist.append(row)
            else:
                log_func("No measurements on sensor detected.")
            c_u_label = Label(window, text="Uplink Interval",
                              font=FONT, bg=IVORY)
            c_u_label.grid(column=0, row=15)
            entry_change_uplink = Entry(
                window, width=40, fg=CHARCOAL, font=FONT_L)
            entry_change_uplink.grid(column=1, row=15)
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

    def _load_measurements(self, window, idy, tablist):
        self._main_frame = Frame(window, borderwidth=8,
                                 relief="ridge", bg="green")
        if idy == "mb":
            self._my_canvas = Canvas(
                self._main_frame, bg=IVORY, width=720, height=240)
            self._my_canvas.grid(column=0, row=0, sticky=NSEW)
            self._main_frame.grid(
                column=3, row=0, rowspan=9, padx=30, columnspan=5)
        else:
            self._my_canvas = Canvas(
                self._main_frame, bg=IVORY, width=625, height=400)
            self._my_canvas.grid(column=0, row=0, sticky=NSEW)
            self._main_frame.grid(column=0, columnspan=3,
                                  row=6, rowspan=9)
        self._second_frame = Frame(self._my_canvas)
        self._second_frame.grid(column=0, row=0)
        sb = Scrollbar(self._main_frame, orient='vertical',
                       command=self._my_canvas.yview)
        sb.grid(column=0, row=0, sticky="NSE")
        self._my_canvas.configure(yscrollcommand=sb.set)
        self._my_canvas.bind('<Configure>', lambda e: self._my_canvas.configure(
            scrollregion=self._my_canvas.bbox("all")))
        self._my_canvas.create_window(
            (0, 0), window=self._second_frame, anchor="nw")
        self._my_canvas.bind_all(
            "<MouseWheel>", lambda: self._on_mousewheel(self._my_canvas))
        if tablist:
            total_rows = len(tablist)
            total_columns = len(tablist[0])
        else:
            tablist = [('Measurement', 'Uplink (min)',
                        'Interval in Mins', 'Sample Count')]
            total_rows = 1
            total_columns = 4
        self._entries = []
        meas_var_list = []
        mb_var_list = []
        self._check_meas = []
        self._check_mb = []
        quart_width = self._my_canvas.winfo_reqwidth() / ((4 * 10)) - 1
        fifth_width = self._my_canvas.winfo_reqwidth() / ((5 * 10)) - 1
        for i in range(total_rows):
            newrow = []
            for j in range(total_columns):
                if idy == 'mb':
                    self._e = Entry(self._second_frame, width=int(fifth_width), fg=BLACK, bg=IVORY,
                                    font=FONT_L)
                else:
                    self._e = Entry(self._second_frame, width=int(quart_width), bg=IVORY, fg=BLACK,
                                    font=FONT_L)
                self._e.grid(row=i, column=j)
                self._e.insert(END, tablist[i][j])
                newrow.append(self._e)
                if total_columns == 5:
                    self._e.configure(
                        state='disabled', disabledbackground="white",
                        disabledforeground="black")
                    if j == 1 and i != 0:
                        reg_hover = Hovertip(
                            self._e, self._get_desc(str(self._e.get())))
                else:
                    self._e.configure(validate="key", validatecommand=(
                        window.register(self._handle_input), '%P', '%d'))
                    if i == 0 or j == 0:
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
                    elif self._e.get() == 'CC1' or self._e.get() == 'CC2' or self._e.get() == 'CC3':
                        self._bind_cc = self._e.bind(
                            "<Button-1>", lambda e: self._cal_cc(e.widget.get()))
                        self._e.configure(disabledforeground="green")
                        self._change_on_hover(self._e)
            self._entries.append(newrow)
            if i != 0:
                if idy == 'mb':
                    self._check_mb.append(IntVar())
                    mb_var_list.append(self._check_mb[i-1])
                    check_m = Checkbutton(self._second_frame, variable=self._check_mb[i-1],
                                          onvalue=i, offvalue=0,
                                          command=lambda: self._check_clicked(window, idy,
                                                                              mb_var_list))
                    check_m.grid(row=i, column=j+1)
                else:
                    self._check_meas.append(IntVar())
                    meas_var_list.append(self._check_meas[i-1])
                    check_meas = Checkbutton(self._second_frame, variable=self._check_meas[i-1],
                                             onvalue=i, offvalue=0,
                                             command=lambda: self._check_clicked(window, idy,
                                                                                 meas_var_list))
                    check_meas.grid(row=i, column=j+1)

    def _remove_reg(self, idy, check):
        ticked = []
        list_of_devs = []
        dev_to_remove = ""
        row = None
        if check:
            for cb in check:
                if cb.get():
                    row = cb.get()
                    ticked.append(row)
            for tick in ticked:
                for i in range(len(self._entries)):
                    if i == tick:
                        if idy == 'mb':
                            log_func(
                                "User attempting to remove modbus register..")
                            device = self._entries[i][0]
                            dev_to_remove = device.get()
                            mb_reg_to_change = self._entries[i][1]
                            mb_reg_to_change = mb_reg_to_change.get()
                            self._dev.do_cmd(f"mb_reg_del {mb_reg_to_change}")
                        else:
                            log_func(
                                "User attempting to set measurement interval 0..")
                            meas_chang = self._entries[i][0]
                            meas_chang = meas_chang.get()
                            inv_chang = self._entries[i][2]
                            uplink_chang = self._entries[i][1]
                            self._dev.do_cmd(f"interval {meas_chang} 0")
                            inv_chang.delete(0, END)
                            inv_chang.insert(0, 0)
                            uplink_chang.delete(0, END)
                            uplink_chang.insert(0, 0)
                        for cb in check:
                            if cb.get() == tick:
                                cb.set(0)
            if idy == 'mb' and len(ticked) > 0:
                self._load_headers(self._modb_fr, "mb", True)
                for i in range(len(self._entries)):
                    dev_entry = self._entries[i][0]
                    dev = dev_entry.get()
                    list_of_devs.append(dev)
                if dev_to_remove not in list_of_devs:
                    self._dev.do_cmd(f"mb_dev_del {dev_to_remove}")

    def _check_clicked(self, window, idy, check):
        for cb in check:
            if cb.get():
                if idy == 'mb':
                    self._del_reg = Button(window, text='Remove',
                                           bg=IVORY, fg=BLACK, font=FONT,
                                           activebackground="green", activeforeground=IVORY,
                                           command=lambda: self._remove_reg(idy, check))
                    self._del_reg.grid(column=6, row=8)
                else:
                    self._rm_int = Button(window, text='Set Interval 0',
                                          bg=IVORY, fg=BLACK, font=FONT,
                                          activebackground="green", activeforeground=IVORY,
                                          command=lambda: self._remove_reg(idy, check))
                    self._rm_int.grid(column=2, row=15)

    def _change_on_hover(self, entry):
        e = entry.get()
        reg_hover = Hovertip(self._e, self._get_desc(entry.get()))

    def _cal_cc(self, widget):
        self._cc_window = Toplevel(self.master)
        self._cc_window.title(f"Current Clamp Calibration {widget}")
        self._cc_window.geometry("650x600")
        self._cc_window.configure(bg=IVORY)
        log_func("User opened window to calibrate current clamp..")

        cc_val_lab = Label(self._cc_window, text="Set Current Clamp Values")
        cc_val_lab.grid(column=0, row=1, pady=10, columnspan=2)
        on_hover = Hovertip(
            cc_val_lab, "Use this page to scale the current clamp")

        self._outer_cc = Entry(self._cc_window, bg=IVORY)
        self._outer_cc.grid(column=0, row=2)
        on_hover = Hovertip(
            self._outer_cc, "Set outer current (Default 100A).")

        outer_cc_lab = Label(self._cc_window, text="Amps")
        outer_cc_lab.grid(column=1, row=2)
        on_hover = Hovertip(
            outer_cc_lab, "Set outer current (Default 100A).")

        self._inner_cc = Entry(self._cc_window, bg=IVORY)
        self._inner_cc.grid(column=0, row=3)
        on_hover = Hovertip(
            self._inner_cc, "Set inner current (Default 50mV).")

        inner_cc_lab = Label(self._cc_window, text="Millivolts")
        inner_cc_lab.grid(column=1, row=3)
        on_hover = Hovertip(
            inner_cc_lab, "Set inner current (Default 50mV).")

        send_btn = Button(self._cc_window,
                          text="Send", command=lambda: self._send_cal(widget),
                          bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        send_btn.grid(column=2, row=3)

        self._cal_terminal = Text(self._cc_window,
                                  height=20, width=80,
                                  bg=BLACK, fg=LIME_GRN)
        self._cal_terminal.grid(column=0, row=4, columnspan=5)
        self._cal_terminal.configure(state='disabled')

        cal_btn = Button(self._cc_window,
                         text="Calibrate ADC", command=self._calibrate,
                         bg=IVORY, fg=BLACK, font=FONT,
                         activebackground="green", activeforeground=IVORY)
        cal_btn.grid(column=0, row=5, pady=10)
        on_hover = Hovertip(
            cal_btn, "Calibrate current clamp midpoint, make sure there is no live current.")

        mp_entry = Entry(self._cc_window, bg=IVORY)
        mp_entry.grid(column=3, row=5)
        on_hover = Hovertip(
            mp_entry, "Manually calibrate ADC midpoint. (Default 2048)")

        mp_btn = Button(self._cc_window, text="Set Midpoint",
                        bg=IVORY, fg=BLACK, font=FONT,
                        activebackground="green", activeforeground=IVORY,
                        command=lambda: self._set_midpoint(mp_entry, widget))
        mp_btn.grid(column=4, row=5)
        on_hover = Hovertip(
            mp_btn, "Manually calibrate ADC midpoint. (Default 2048)")
        self._fill_cc_term(widget)

        self._cal_label = Label(self._cc_window, text="")
        self._cal_label.grid(column=3, row=6)

        help_cc = Label(self._cc_window, text="Help")
        help_cc.grid(column=4, row=7)
        help_hover = Hovertip(help_cc, '''
            Use this page to calibrate each phase of current.\t\n
            Here you can set Exterior and Interior values which are defaulted to 100A and 50mV respectively.\t\n
            You must set the midpoint when first configuring each phase, achieve this by simply selecting calibrate ADC.\t\n
            There is also the option to set the midpoint manually. A typical value is around 2030-2048.\t\n
            ''')

    def _fill_cc_term(self, widget):
        cc_gain = self._dev.do_cmd_multi("cc_gain")
        self._cal_terminal.configure(state='normal')
        self._cal_terminal.delete('1.0', END)
        if cc_gain:
            for i in cc_gain:
                if widget in i:
                    self._cal_terminal.insert(INSERT, i + "\n")
            midpoint = self._dev.do_cmd_multi(f"cc_mp {widget}")
            for v in midpoint:
                self._cal_terminal.insert(INSERT, "\n" + v + "\n")
            self._cal_terminal.configure(state='disabled')
        else:
            tkinter.messagebox.showerror(
                "Error", "Press OK to reopen window..")
            self._cc_window.destroy()
            self._cal_cc(widget)

    def _set_midpoint(self, entry, widget):
        log_func("User attempting to set current clamp midpoint..")
        v = entry.get()
        try:
            v = int(v)
        except ValueError:
            self._cal_label['text'] = "You must enter an integer"
        else:
            mp = entry.get()
            set_mp = self._dev.do_cmd_multi(f"cc_mp {mp} {widget}")
            cc_gain = self._dev.do_cmd_multi("cc_gain")
            self._fill_cc_term(widget)
            self._cal_label['text'] = ""

    def _calibrate(self):
        log_func("User attempting to calibrate ADC automatically..")
        dev_off = tkinter.messagebox.askyesno(
            "Calibrate", "Has the current been switched off?", parent=self._cc_window)
        if dev_off:
            cal = self._dev.do_cmd("cc_cal")
            self._cal_terminal.configure(state='normal')
            self._cal_terminal.delete('1.0', END)
            self._cal_terminal.insert(INSERT, cal + "\n")
            self._cal_terminal.configure(state='disabled')
        else:
            tkinter.messagebox.showinfo(
                "", "Turn off live current.", parent=self._cc_window)

    def _send_cal(self, widget):
        log_func("User attempting to manually calibrate outer and inner cc...")
        if self._outer_cc.get() and self._inner_cc.get():
            outer = self._outer_cc.get()
            inner = self._inner_cc.get()
            try:
                int(outer) and int(inner)
                phase = widget[2]
                self._cal_terminal.configure(state='normal')
                self._cal_terminal.delete('1.0', END)
                gain = self._dev.do_cmd(f"cc_gain {phase} {outer} {inner}")
                self._fill_cc_term(widget)
            except:
                tkinter.messagebox.showerror(
                    "Error", "Outer and Inner currents must be integers", parent=self._cc_window)
        else:
            tkinter.messagebox.showerror(
                "Error", "Fill in missing fields.", parent=self._cc_window)

    def _manual_config(self):
        man_window = Toplevel(self.master)
        man_window.title("List of Commands")
        man_window.geometry("600x800")
        man_window.configure(bg=IVORY)
        cmd_label = Label(man_window, text="",
                          font=FONT,
                          fg=IVORY, bg=CHARCOAL)
        cmd_label.pack()
        line = self._dev.do_debug("?")
        newline = self._dev.imp_readlines()
        self._write_terminal_cmd(newline, self._terminal)
        list_of_cmds = ""
        for cmd in CMDS:
            cmd = str(cmd)
            list_of_cmds += cmd
            list_of_cmds += "\n"
        cmd_label.config(text=list_of_cmds)

    def _enter_cmd(self, cmd):
        cmd = cmd.get()
        self._dev.do_debug(cmd)
        newline = self._dev.imp_readlines()
        self._write_terminal_cmd(newline, self._terminal)

    def _pop_lora_entry(self):
        dev_eui = self._dev.do_cmd_multi("comms_config dev-eui")
        if dev_eui:
            eui_op = dev_eui[0].split()[2]
            self._eui_entry.insert(0, eui_op)

        app_key = self._dev.do_cmd_multi("comms_config app-key")
        if app_key:
            app_op = app_key[0].split()[2]
            self._app_entry.insert(0, app_op)

    def _get_lora_status(self):
        status = self._dev.do_cmd_multi("comms_conn")
        if status:
            conn = status[0].split()[0]
            if conn == '1':
                self._lora_status.configure(text="Connected", fg="green")
            else:
                self._lora_status.configure(text="Disconnected", fg="red")
        else:
            self._lora_status.configure(text="Disconnected", fg="red")

    def _open_lora_config(self, frame):
        img = Image.open(R_LOGO)
        icon = ImageTk.PhotoImage(img)
        self._conn_fr.img_list.append(icon)
        lora_c_label = Label(frame, text="LoRaWAN Configuration",
                             bg=IVORY, font=FONT_L)
        lora_c_label.grid(column=5, row=6, sticky="E", columnspan=2)

        lora_label = Label(frame, text="Device EUI: ",
                           bg=IVORY, font=FONT)
        lora_label.grid(column=4, row=7, sticky="E")

        self._eui_entry = Entry(frame, font=FONT, bg=IVORY)
        self._eui_entry.grid(column=5, row=7, sticky="NSEW", columnspan=2)

        add_eui_btn = Button(frame, image=icon,
                             command=self._insert_eui, bg=IVORY, fg=BLACK, font=FONT,
                             activebackground="green", activeforeground=IVORY)
        add_eui_btn.grid(column=7, row=7, sticky="NSEW")

        app_label = Label(frame, text="Application Key: ",
                          bg=IVORY, font=FONT)
        app_label.grid(column=4, row=8, sticky="E", padx=(30, 0))

        self._app_entry = Entry(frame, font=FONT, bg=IVORY)
        self._app_entry.grid(column=5, row=8, sticky="NSEW", columnspan=2)

        add_app_btn = Button(frame, image=icon,
                             command=self._insert_lora,
                             bg=IVORY, fg=BLACK, font=FONT,
                             activebackground="green", activeforeground=IVORY)
        add_app_btn.grid(column=7, row=8, sticky="NSEW")

        send_btn = Button(frame, text="Send",
                          command=self._save_cmd, bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        send_btn.grid(column=7, row=9, sticky="NSEW")

        save_btn = Button(frame, text="Save",
                          command=lambda: self._dev.do_cmd("save"), bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        save_btn.grid(column=7, row=10, sticky="NSEW")

        lora_l = Label(frame, text="Status: ",
                       bg=IVORY, font=FONT)
        lora_l.grid(column=4, row=9, sticky="E")

        self._lora_status = Label(frame, text="",
                                  bg=IVORY, font=FONT)
        self._lora_status.grid(column=5, row=9)

        self._lora_confirm = Label(frame, text="",
                                   font=FONT, bg=IVORY)
        self._lora_confirm.grid(column=6, row=9, sticky="NSEW")

        self._get_lora_status()

    def _save_cmd(self):
        log_func("User attempting to save new lora configuration..")
        dev_eui = self._eui_entry.get()
        app_key = self._app_entry.get()
        if len(dev_eui) == 16 and len(app_key) == 32:
            dev_output = self._dev.do_cmd(f"comms_config dev-eui {dev_eui}")
            app_output = self._dev.do_cmd(f"comms_config app-key {app_key}")
            self._lora_confirm.configure(text="Configuration sent.")
        else:
            self._lora_confirm.configure(
                text="Missing fields or bad character limit.")

    def _insert_lora(self):
        app_key = self._dev.write_lora()
        self._app_entry.delete(0, END)
        self._app_entry.insert(0, app_key)

    def _insert_eui(self):
        dev_eui = self._dev.write_eui()
        self._eui_entry.delete(0, END)
        self._eui_entry.insert(0, dev_eui)

    def _on_closing(self):
        debug = None
        if self._connected == True:
            try:
                debug = self._dev.imp_readlines()
            except Exception as e:
                log_func(e)
            self._dev.do_cmd("debug 0")
            self._close_save(root)
        else:
            root.destroy()

    def _main_modbus_w(self):
        name = Label(self._modb_fr, text="Modbus Device Templates",
                     font=FONT, bg=IVORY)
        name.grid(column=1, row=0)

        self.template_list = Listbox(
            self._modb_fr, height=10, exportselection=False)
        self.template_list.grid(column=1, ipadx=150,
                                ipady=50, row=1, rowspan=7)
        self.template_list.bind("<<ListboxSelect>>", self._callback)

        delete_button = Button(self._modb_fr, text="Delete",
                               command=lambda: self._delete_template(
                                   self.template_list),
                               width=5, bg=IVORY, fg=BLACK, font=FONT,
                               activebackground="green", activeforeground=IVORY)
        delete_button.grid(column=2, row=1)
        tt_del_btn = Hovertip(delete_button, 'Remove template.')

        edit_template_button = Button(self._modb_fr, text="Edit", width=5,
                                      command=lambda: self._add_template_w(
                                          'edit'),
                                      bg=IVORY, fg=BLACK, font=FONT,
                                      activebackground="green", activeforeground=IVORY)
        edit_template_button.grid(column=2, row=2)
        tt_edit_btn = Hovertip(edit_template_button,
                               'Edit template.')

        add_template_button = Button(self._modb_fr, text="Add",
                                     command=lambda: self._add_template_w(
                                         None),
                                     width=5, bg=IVORY, fg=BLACK, font=FONT,
                                     activebackground="green", activeforeground=IVORY)
        add_template_button.grid(column=2, row=3)
        tt_add_btn = Hovertip(add_template_button, 'Create a new template.')

        apply_template_button = Button(self._modb_fr, text="Apply", width=5,
                                       command=lambda: self._write_to_dev(
                                           self.template_list),
                                       bg=IVORY, fg=BLACK, font=FONT,
                                       activebackground="green", activeforeground=IVORY)
        apply_template_button.grid(column=2, row=4)
        tt_apply_btn = Hovertip(apply_template_button,
                                'Write template to device.')

        copy_template_button = Button(self._modb_fr, text="Copy", width=5,
                                      command=lambda: self._add_template_w(
                                          " (copy)"),
                                      bg=IVORY, fg=BLACK, font=FONT,
                                      activebackground="green", activeforeground=IVORY)
        copy_template_button.grid(column=2, row=5)
        tt_copy_btn = Hovertip(copy_template_button,
                               'Duplicate template.')

        revert_template_button = Button(self._modb_fr, text="Revert", width=5,
                                        command=self._reset_listb,
                                        bg=IVORY, fg=BLACK, font=FONT,
                                        activebackground="green", activeforeground=IVORY)
        revert_template_button.grid(column=2, row=6)
        tt_revert_btn = Hovertip(
            revert_template_button, 'Undo unsaved changes.')

        save_template_button = Button(self._modb_fr, text="Save", width=5,
                                      command=self._send_to_save,
                                      bg=IVORY, fg=BLACK, font=FONT,
                                      activebackground="green", activeforeground=IVORY)
        save_template_button.grid(column=2, row=7)
        tt_save_btn = Hovertip(save_template_button, 'Save all changes.')

        unit_id = Label(self._modb_fr, text="Modbus Registers on Template",
                        font=FONT, bg=IVORY)
        unit_id.grid(column=1, row=8)

        self.template_regs_box = Listbox(self._modb_fr, exportselection=False)
        self.template_regs_box.grid(
            column=1, ipadx=150, ipady=50, row=9, rowspan=4)

        del_reg_button = Button(self._modb_fr, text="Delete", width=5,
                                command=lambda:
                                self._del_reg_tmp(self.template_regs_box, None,
                                                  self.template_list,
                                                  None, 'temp_reg_box'),
                                bg=IVORY, fg=BLACK, font=FONT,
                                activebackground="green", activeforeground=IVORY)
        del_reg_button.grid(column=2, row=9)
        tt_del_r_btn = Hovertip(del_reg_button, 'Remove register.')

        shift_up_button = Button(self._modb_fr, text="▲", width=5,
                                 command=lambda:
                                 self._shift_up(self.template_regs_box,
                                                self.template_list,
                                                None, 'temp_reg_box'),
                                 bg=IVORY, fg=BLACK, font=FONT,
                                 activebackground="green", activeforeground=IVORY)
        shift_up_button.grid(column=2, row=10)
        tt_sh_u_btn = Hovertip(shift_up_button, 'Shift up')

        shift_down_button = Button(self._modb_fr, text="▼", width=5,
                                   command=lambda:
                                   self._shift_down(self.template_regs_box,
                                                    self.template_list,
                                                    None, 'temp_reg_box'),
                                   bg=IVORY, fg=BLACK, font=FONT,
                                   activebackground="green", activeforeground=IVORY)
        shift_down_button.grid(column=2, row=11)
        tt_sh_d_btn = Hovertip(shift_down_button, 'Shift down')

        self._current_mb = Label(
            self._modb_fr, text="Current Modbus Settings on OSM", font=FONT, bg=IVORY)
        self._current_mb.grid(column=3, row=0, columnspan=3)
        self.template_list.curselection()
        with self.db.conn:
            data = self.db.cur.execute(GET_TMP_N)
            for row in data:
                dev = ''.join(row)
                self.template_list.insert(0, dev)
                write_dev_to_yaml = self.db.get_all_info(dev)
                with open(PATH + '/yaml_files/modbus_data.yaml', 'a') as f:
                    yaml.dump(write_dev_to_yaml, f)

        img = Image.open(PARAMS)
        param_logo = ImageTk.PhotoImage(img)
        self._modb_fr.img_list = []
        dev_lab = Label(self._modb_fr, image=param_logo, bg=IVORY)
        dev_lab.grid(column=3, row=9, rowspan=5, padx=20)
        self._modb_fr.img_list.append(param_logo)

        canv = Canvas(self._modb_fr)
        canv.grid(column=0, row=15, columnspan=10, pady=70)

        img_os = Image.open(OPEN_S)
        os_logo = ImageTk.PhotoImage(img_os)
        os_lab = Label(canv, image=os_logo, bg=IVORY)
        os_lab.grid(column=0, row=0)
        self._modb_fr.img_list.append(os_logo)
        root.update()
        self._load_headers(self._modb_fr, "mb", True)

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
                            device_name = device['device_name']
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
                curr_dev = []
                self._modbus = self._dev.get_modbus()
                if self._modbus.devices:
                    for i in self._modbus.devices:
                        unit_id = i.unit
                        curr_dev.append(unit_id)
                regs = self.db.get_modbus_template_regs(chosen_template)
                if regs:
                    reg_n = []
                    if curr_dev:
                        for i in self._modbus.devices:
                            for v in i.regs:
                                reg_conf = v
                                reg_name = (str(reg_conf).split(',')[0])
                                reg_n.append(reg_name)
                        for i in regs:
                            unit_id = i[0]
                            bytes = i[1]
                            dev_name = i[6]
                            hex_addr = i[2]
                            func_type = i[3]
                            data_type = i[4]
                            reg_name = i[5]
                            if i == regs[0] and unit_id not in curr_dev:
                                dev_add = self._dev.do_cmd(
                                    f"mb_dev_add {unit_id} {bytes} {dev_name}")
                            if reg_name not in reg_n:
                                reg_add = self._dev.do_cmd(
                                    f"mb_reg_add {unit_id} {hex_addr} {func_type} {data_type} {reg_name}")
                    else:
                        for i in regs:
                            unit_id = i[0]
                            bytes = i[1]
                            dev_name = i[6]
                            hex_addr = i[2]
                            func_type = i[3]
                            data_type = i[4]
                            reg_name = i[5]
                            if i == regs[0]:
                                dev_add = self._dev.do_cmd(
                                    f"mb_dev_add {unit_id} {bytes} {dev_name}")
                                reg_add = self._dev.do_cmd(
                                    f"mb_reg_add {unit_id} {hex_addr} {func_type} {data_type} {reg_name}")
                            else:
                                reg_add = self._dev.do_cmd(
                                    f"mb_reg_add {unit_id} {hex_addr} {func_type} {data_type} {reg_name}")
                    self._load_headers(self._modb_fr, "mb", True)
                    tkinter.messagebox.showinfo(
                        "Device Written", "Go to debug mode to monitor data.")
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
    root.title(f"Open Smart Monitor Configuration {tag}")
    root.resizable(True, True)
    root.configure(bg="lightgrey")
    root.protocol("WM_DELETE_WINDOW", root._on_closing)
    logging.basicConfig(
        format='[%(asctime)s.%(msecs)06d] GUI : %(message)s',
        level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S')
    binding.set_debug_print(binding.default_print)
    root.mainloop()
