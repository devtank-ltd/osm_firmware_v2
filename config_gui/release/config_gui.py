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
from multiprocessing import Process, Queue
from tkinter.filedialog import askopenfilename
import threading
import traceback
from modbus_funcs import modbus_funcs_t
import modbus_db
from modbus_db import modb_database_t, find_path
from PIL import ImageTk, Image
import logging
import platform
import signal
from stat import *
from gui_binding_interface import binding_interface_svr_t, binding_interface_client_t

MB_DB = modbus_db
FW_PROCESS = False
THREAD = threading.Thread
GET_REG_DESC = MB_DB.GET_REG_DESC

LINUX_OSM_TTY = "/tmp/osm/UART_DEBUG_slave"

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
GET_TMP_N = MB_DB.GET_TMP_N
GET_TEMP_ID = MB_DB.GET_TEMP_ID
GET_UNIT_IDS = MB_DB.GET_UNIT_IDS
ICONS_T   =    PATH + "/osm_pictures/logos/icons-together.png"
DVT_IMG   =    PATH + "/osm_pictures/logos/OSM+Powered.png"
OSM_1     =    PATH + "/osm_pictures/logos/lora-osm.png"
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


def _binding_process_cb(binding_in, binding_out):
    binding_svr = binding_interface_svr_t()
    binding_svr.run_forever(binding_in, binding_out)


class config_gui_window_t(Tk):
    def __init__(self):
        super().__init__()
        signal.signal(signal.SIGINT, handle_exit)
        self.db = modb_database_t(modbus_db.find_path())
        self._modb_funcs = modbus_funcs_t(self.db)
        self._connected = False
        self._changes = False
        self._widg_del = False
        self.count=0
        with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
            pass
        with open(PATH + '/yaml_files/del_file.yaml', 'w') as df:
            pass

        
        self.binding_out = Queue()
        self.binding_in = Queue()

        self.binding_process = Process(target=_binding_process_cb, args=(self.binding_in, self.binding_out))
        self.binding_process.start()
        self.binding_interface = binding_interface_client_t(self.binding_in, self.binding_out)

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
        self.binding_interface.process_message()
        self.after(1000, lambda: self._check_binding_comms())


    def _on_connect(self):
        self.dev_sel = self._dev_dropdown.get()
        if self.dev_sel:
            log_func("User attempting to connect to device.. : " + self.dev_sel)
            self._downl_fw.pack_forget()
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
                self._adv_fr, text='Advanced Config')
            self._notebook.add(
                self._modb_fr, text="Modbus Config")
            self._notebook.add(self._debug_fr, 
            text="Debug Mode")
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
        self._load_meas_l = Label(
            self._main_fr, 
            text="Current Measurements on OSM",
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
        self._dbg_open = False
        self._notebook.bind("<<NotebookTabChanged>>",
                            lambda e: self._tab_changed(e,
                                                        self._main_fr,
                                                        self._notebook))
        self._load_headers(self._main_fr, "rif", True)

        self._modbus    = None
        self._sens_meas = None
        self.ser_op     = None
        self.fw_version = None
        self.eui_op     = None
        self.app_op     = None

        self.binding_interface.get_interval_min(self._on_get_interval_min_done_cb)
        self.binding_interface.get_serial_num(self._on_get_serial_num_done_cb)
        self.binding_interface.get_fw_version(self._on_get_fw_version_done_cb)
        self.binding_interface.get_dev_eui(self._on_get_dev_eui_done_cb)
        self.binding_interface.get_app_key(self._on_get_app_key_done_cb)

    def _on_get_serial_num_done_cb(self, resp):
        self.ser_op = resp[1]
        self._sensor_name.configure(
            text="Serial Number - " + self.ser_op,
            bg=IVORY, font=FONT)
        pass

    def _on_get_fw_version_done_cb(self, resp):
        fw_version = resp[1]
        self.fw_version = "-".join(fw_version.split("-")[0:2])
        self._fw_version_body.configure(
            text="Current Firmware Version - " + self.fw_version,
            bg=IVORY, font=FONT)

    def _on_get_dev_eui_done_cb(self, resp):
        self.dev_eui = resp[1]
        self._pop_lora_entry()

    def _on_get_app_key_done_cb(self, resp):
        self.app_key = resp[1]
        self._pop_lora_entry()

    def _on_get_interval_min_done_cb(self, resp):
        self._interval_min = resp[1]
        return self._interval_min

    def _tab_changed(self, event, frame, notebook):
        slction = notebook.select()
        log_func(f"User changed to tab {slction}.")
        if slction == '.!notebook.!frame4' and self.modbus_opened == False:
            with open(PATH + '/yaml_files/modbus_data.yaml', 'w') as f:
                pass
            self.modbus_opened = True
            self._main_modbus_w()
        elif slction == '.!notebook.!frame5' and self._dbg_open == False:
            self._dbg_open = True
            if self.count == 0:
                self._thread_debug()
            else:
                self._reload_debug_lines()
        if slction != '.!notebook.!frame5' and self._dbg_open == True:
            self._dbg_open = False

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
            self._dev.change_samplec(meas_chang, widget_val)
            samp_chang.delete(0, END)
            samp_chang.insert(0, widget_val)
        elif int(widget_id) % 2 == 0:
            self._dev.change_interval(meas_chang, widget_val)
            if meas_chang == 'TMP2' and widget_val != '0':
                self._dev.change_interval("CNT1", "0")
                self._update_meas_tab('CNT1')
            elif meas_chang == 'CNT1' and widget_val != '0':
                self._dev.change_interval("TMP2" ,"0")
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
        self._dev.change_interval(meas_chang, min)
        uplink_chang.delete(0, END)
        uplink_chang.insert(0, int(min))
        # if user changes interval in mins for pulsecount or one wire
        if meas_chang == 'CNT1' and widget_val != '0':
            self._dev.change_interval("TMP2", "0")
            self._update_meas_tab('TMP2')
        elif meas_chang == 'TMP2' and widget_val != '0':
            self._dev.change_interval("CNT1", "0")
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

    def _get_ios_pin_obj(self, meas):
        if meas == "TMP2" or meas == "CNT1":
            return self._dev.ios[4]
        elif meas == "TMP3" or meas == "CNT2":
            return self._dev.ios[5]
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
                pin_obj.disable_io()
            pin_obj.activate_io(inst, pullup)
        elif cmd == 'disable':
            # disable io
            pin_obj.disable_io()
        self.ios_page .destroy()
        self._open_ios_w(meas)


    def _open_ios_w(self, meas):
        self.ios_page = Toplevel(self.master)
        self.ios_page.title(f"Set IO {meas}")
        self.ios_page.geometry("405x500")
        self.ios_page.configure(bg=IVORY)
        log_func("User opened IO window..")
        pin_obj = self._get_ios_pin_obj(meas)
        assert pin_obj
        pin_is = pin_obj.active_as()
        if not meas.startswith('TMP'):
            io_d_label = Label(
                self.ios_page,
                text="Set Pull Up or Pull Down")
            io_d_label.pack()
            self._up_d_non = Combobox(self.ios_page)
            self._up_d_non.pack()
            self._up_d_non['values'] = ('Up', 'Down', 'None')
            if meas == 'CNT1':
                self._up_d_non.set('Up')
                self._up_d_non.configure(state='disabled')
            else:
                pull = pin_obj.active_pull()
                if pull:
                    pull = pull.lower()
                    if pull[0] == 'u':
                        self._up_d_non.set('Up')
                    elif pull[0] == 'd':
                        self._up_d_non.set('Down')
                    else:
                        self._up_d_non.set('None')
                else:
                    self._up_d_non.set('None')
                if pin_is == meas:
                    self._up_d_non.configure(state='disabled')
        if pin_is == meas:
            dis_check = Button(self.ios_page,
                               text="Disable",
                               command=lambda: self._do_ios_cmd('disable', meas, pin_obj, pin_is),
                               bg=IVORY, fg=BLACK, font=FONT,
                               activebackground="green", activeforeground=IVORY)
            dis_check.pack()
        else:
            en_check = Button(self.ios_page,
                              text="Enable",
                              command=lambda: self._do_ios_cmd('enable', meas, pin_obj, pin_is),
                              bg=IVORY, fg=BLACK, font=FONT,
                              activebackground="green", activeforeground=IVORY)
            en_check.pack()
        self._ios_label = Label(self.ios_page, text="",
            bg=IVORY, font=FONT)
        self._ios_label.pack()
        self._set_ios_label(meas, pin_obj, pin_is)

    def _thread_debug(self):
        self.count = self.count + 1
        self._open_debug_w(self._debug_fr)
        log_func(f"User switched to tab 'Debug Mode'.")
        self._load_debug_meas(self._debug_fr)

    def _open_debug_w(self, frame):
        frame.columnconfigure([0,1,2,3,4,5,6], weight=1)
        frame.rowconfigure([0,1,2,3,4,5,6], weight=1)
        l_img = Image.open(OSM_BG)
        leaf_logo = ImageTk.PhotoImage(l_img)
        self._debug_fr.img_list = []
        self._debug_fr.img_list.append(leaf_logo)
        self._leaf_lab = Label(
            self._debug_fr, image=leaf_logo, bg=IVORY)
        self._leaf_lab.pack()
        self._debug_fr.bind('<Configure>', lambda e: self._resize_image(
            e, self._leaf_lab, l_img, self._debug_fr
        ))

        debug_btn = Button(frame,
                           text="Activate/Disable Debug Mode",
                           command=lambda: self._dev.do_cmd("debug_mode"),
                           bg=IVORY, fg=BLACK, font=FONT,
                           activebackground="green", activeforeground=IVORY)
        debug_btn.grid(column=0, row=1, sticky=S)

        self._dbg_terml = Text(frame,
                               bg=BLACK, fg=LIME_GRN,
                               borderwidth=10, relief="sunken")
        self._dbg_terml.grid(column=0, row=2, sticky=NS)
        self._dbg_terml.configure(state='disabled')

        self._debug_first_fr = Frame(frame, bg="green", borderwidth=8,
                                     relief="ridge")
        self._debug_first_fr.grid(column=3, row=2, sticky="EW")

        self._dbg_canv = Canvas(
            self._debug_first_fr)
        self._dbg_canv.grid(column=0, row=0, sticky=NSEW)


    def _load_debug_meas(self, frame):
        hdrs = [('Measurement', 'Value')]
        meas = []
        self._dbg_sec_fr = Frame(self._dbg_canv)
        self._dbg_sec_fr.pack(expand=True, fill='both')
        self._debug_first_fr.columnconfigure(0, weight=1)

        if self._sens_meas:
            for m in self._sens_meas:
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
                    debug_e = Entry(self._dbg_sec_fr,
                                    bg=IVORY, fg=CHARCOAL,
                                    font=('Arial', 14, 'bold'))
                    debug_e.grid(row=i, column=j, sticky=EW)
                    self._dbg_sec_fr.columnconfigure(j, weight=1)
                    debug_e.insert(END, hdrs[i][j])
                    debug_e.configure(state='disabled',
                                      disabledbackground=IVORY,
                                      disabledforeground=BLACK)
                    newrow.append(debug_e)
                self._deb_entries.append(newrow)
        self._dbg_canv.create_window(
            (0, 0), window=self._dbg_sec_fr, anchor="nw", tags="frame")
        self._dbg_canv.bind_all(
            "<MouseWheel>", lambda: self._on_mousewheel(self._dbg_canv))
        debug_sb = Scrollbar(
            self._debug_first_fr, orient='vertical', command=self._dbg_canv.yview)
        debug_sb.grid(column=0, row=0, sticky="NSE")

        self._dbg_canv.configure(yscrollcommand=debug_sb.set)
        self._dbg_canv.bind('<Configure>', lambda e: self._on_canvas_config(
            e, self._dbg_canv
        ))
        self._reload_debug_lines()

    def _reload_debug_lines(self):
        if self._dbg_open:
            dbg_list = self._dev.dbg_readlines()
            for d in dbg_list:
                if d:
                    res = self._debug_parse.parse_msg(d)
                    if res:
                        dbg_meas = res[0]
                        dbg_val = res[1]
                        if dbg_val != False:
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
            self._dbg_terml.after(3000, self._reload_debug_lines)

    def _on_mousewheel(self, event, canvas):
        canvas.yview_scroll(-1*(event.delta/120), "units")

    def _get_desc(self, reg):
        desc = STD_MEASUREMENTS_DESCS.get(reg, None)
        spec_m = ['CC1', 'CC2', 'CC3', 'TMP2', 'CNT1', 'CNT2']
        if reg in spec_m:
            self._e.configure(cursor='hand2')
        elif desc is None:
            description = self.db.conn.execute(GET_REG_DESC(reg))
            if desc:
                desc = description.fetchone()[0]
        return desc

    def _change_uplink(self, frame, widg):
        if int(widg.get()):
            uplink = int(widg.get())
            if uplink > 255:
                self._dev.interval_mins = 255
            elif uplink < 3:
                self._dev.interval_mins = 3
            else:
                self._dev.interval_mins = uplink
            self._load_headers(frame, "rif", False)
        else:
            tkinter.messagebox.showerror(
                "Error", "Uplink interval must be an integer")

    def _handle_input(self, in_str, acttyp):
        if acttyp == '1':
            if not in_str.isdigit():
                return False
        return True
    
    def _request_modbus(self):
        mmnts = []
        m = self._dev.get_modbus()
        devs = m.devices
        if devs:
            for dev in devs:
                for reg in dev.regs:
                    i = (str(reg).split(','))
                    i.insert(0, str(dev.name))
                    mmnts.append(i)
        return mmnts
    
    def _refresh_mb(self):

        self._modbus = ast.literal_eval(modb_proc.stdout.replace(
            "([<_io.TextIOWrapper name='<stdin>' mode='r' encoding='utf-8'>], [], [])\n", ""))
        self._modbus = self._request_modbus()
        return self._modbus
        
    def _load_headers(self, window, idy, get_mins):
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

    def _load_modbus(self, updated):
        if updated:
            modbus = self._refresh_mb()
        else:
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
                        self._mbe, self._get_desc(str(self._e.get())))
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
                        'Interval in Mins', 'Sample Count')]
            total_rows = 1
            total_columns = 4
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
                self._check_meas.append(IntVar())
                meas_var_list.append(self._check_meas[i-1])
                check_meas = Checkbutton(self._second_frame, variable=self._check_meas[i-1],
                                            onvalue=i, offvalue=0,
                                            command=lambda: self._check_clicked(window, idy,
                                                                                meas_var_list))
                check_meas.grid(row=i, column=j+1, padx=(0,15))
    

    def _remove_mb_reg(self, idy, check):
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
                for i in range(len(self.mb_entries)):
                    if i == tick:
                            log_func(
                                "User attempting to remove modbus register..")
                            device = self.mb_entries[i][0]
                            dev_to_remove = device.get()
                            mb_reg_to_change = self.mb_entries[i][1]
                            mb_reg_to_change = mb_reg_to_change.get()
                            self._dev.modbus_reg_del(mb_reg_to_change)
            self._load_modbus(True)
            for i in range(len(self._entries)):
                dev_entry = self._entries[i][0]
                dev = dev_entry.get()
                list_of_devs.append(dev)
            if dev_to_remove not in list_of_devs:
                self._dev.modbus_dev_del(dev_to_remove)

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
                        self._dev.change_interval(meas_chang, 0)
                        inv_chang.delete(0, END)
                        inv_chang.insert(0, 0)
                        uplink_chang.delete(0, END)
                        uplink_chang.insert(0, 0)
                    for cb in check:
                        if cb.get() == tick:
                            cb.set(0)

    def _check_clicked(self, window, idy, check):
        for cb in check:
            if cb.get():
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
                        text="Calibrate ADC", 
                        command=lambda:self._calibrate(widget),
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

        self._cal_label = Label(self._cc_window, text="",
        bg=IVORY)
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
        cc_gain = self._dev.print_cc_gain
        self._cal_terminal.configure(state='normal')
        self._cal_terminal.delete('1.0', END)
        if cc_gain:
            for i in cc_gain:
                if widget in i:
                    self._cal_terminal.insert(INSERT, i + "\n")
            midpoint = self._dev.get_midpoint(widget)
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
            set_mp = self._dev.update_midpoint(mp, widget)
            cc_gain = self._dev.print_cc_gain
            self._fill_cc_term(widget)
            self._cal_label['text'] = ""

    def _calibrate(self, widget):
        log_func("User attempting to calibrate ADC automatically..")
        dev_off = tkinter.messagebox.askyesno(
            "Calibrate", "Has the current been switched off?", parent=self._cc_window)
        if dev_off:
            cal = self._dev.current_clamp_calibrate()
            self._fill_cc_term(widget)
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
                gain = self._dev.set_outer_inner_cc(phase, outer, inner)
                self._fill_cc_term(widget)
            except:
                tkinter.messagebox.showerror(
                    "Error", "Outer and Inner currents must be integers", parent=self._cc_window)
        else:
            tkinter.messagebox.showerror(
                "Error", "Fill in missing fields.", parent=self._cc_window)

    def _enter_cmd(self, cmd):
        self._terminal.configure(state='normal')
        self._terminal.delete('1.0', END)
        cmd = cmd.get()
        cmd = self._dev.do_cmd_multi(cmd)
        if cmd:
            for cm in cmd:
                self._write_terminal_cmd(cm, self._terminal)

    def _pop_lora_entry(self):
        if self.eui_op:
            self._eui_entry.insert(0, self.eui_op)
        if self.app_op:
            self._app_entry.insert(0, self.app_op)

    def _get_lora_status(self):
        conn = self._dev.comms_conn.value
        if conn is not None:
            if conn:
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

        lora_label = Label(frame, text="Dev EUI: ",
                           bg=IVORY, font=FONT)
        lora_label.grid(column=5, row=7, sticky="E")

        self._eui_entry = Entry(frame, font=FONT, bg=IVORY)
        self._eui_entry.grid(column=6, row=7, sticky="NSEW")

        add_eui_btn = Button(frame, image=icon,
                             command=self._insert_eui, bg=IVORY, fg=BLACK, font=FONT,
                             activebackground="green", activeforeground=IVORY)
        add_eui_btn.grid(column=7, row=7, sticky="NSEW")

        app_label = Label(frame, text="AppKey: ",
                          bg=IVORY, font=FONT)
        app_label.grid(column=5, row=8, sticky="E")

        self._app_entry = Entry(frame, font=FONT, bg=IVORY)
        self._app_entry.grid(column=6, row=8, sticky="NSEW")

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
                          command=lambda: self._dev.save(), bg=IVORY, fg=BLACK, font=FONT,
                          activebackground="green", activeforeground=IVORY)
        save_btn.grid(column=7, row=10, sticky="NSEW")

        lora_l = Label(frame, text="Status: ",
                       bg=IVORY, font=FONT)
        lora_l.grid(column=5, row=9, sticky="E")

        self._lora_status = Label(frame, text="",
                                  bg=IVORY, font=FONT)
        self._lora_status.grid(column=6, row=9, sticky=W)

        self._lora_confirm = Label(frame, text="",
                                   font=FONT, bg=IVORY)
        self._lora_confirm.grid(column=6, row=9, sticky="E")

        self._get_lora_status()

    def _save_cmd(self):
        log_func("User attempting to save new lora configuration..")
        dev_eui = self._eui_entry.get()
        app_key = self._app_entry.get()
        if len(dev_eui) == 16 and len(app_key) == 32:
            self._dev.dev_eui = dev_eui
            self._dev.app_key = app_key
            self._lora_confirm.configure(text="Configuration sent.")
            self._pop_lora_entry()
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
            self._dev._set_debug(0)
            self._close_save(root)
        else:
            root.destroy()


        
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
            # canv.create_image(0, 0, image=new_image, anchor="nw")
            label.configure(image=new_image)
            frame.img_list.append(new_image)
    
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
