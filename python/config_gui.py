from asyncio import base_tasks
from cgitb import text
from logging import exception
from shutil import ExecError
from subprocess import CompletedProcess
from tkinter import *
from tkinter.ttk import Combobox
from click import command
import serial.tools.list_ports
import serial
import re
import io
import random
import string
import binding
import time

class Window(Frame):
    def __init__(self, master=None):
        Frame.__init__(self, master)
        self.master = master

        usb_label = Label(master, text="Select a device", pady=1)
        usb_label.grid(column=1, row=0)

        returned_ports = []
        active_ports = serial.tools.list_ports.comports(include_links=True)
        for item in active_ports:
            returned_ports.append(item.device)

        self.dropdown = Combobox(master, values = returned_ports)
        for item in active_ports:
            if "tty" in item.device:
                self.dropdown.set(item.device)
        self.dropdown.grid(column=1, row=1)

        self.dev = binding.dev_t()

        self.device_to_use = self.dropdown.get()

        self.wipe_dev = Button(master, text="Wipe device", command = self.dev.wipe_clean)
        self.wipe_dev.grid(column=0, row=2)       

        self.debug_mb_btn = Button(master, text="Set debug to modbus", command=self.dev.debug_mb)
        self.debug_mb_btn.grid(column=0, row=3)

        self.man_config_btn = Button(master, text="Configure device manually", command=self.manual_config)
        self.man_config_btn.grid(column=0, row=4)        

        self.mb_setup_btn = Button(master, text="See modbus setup", command=self.dev.mb_log)
        self.mb_setup_btn.grid(column=1, row=2)

        self.add_dev_btn = Button(master, text="Add RI-F220 device", command=self.dev.add_rif_dev)
        self.add_dev_btn.grid(column=1, row=3)

        self.add_rif_btn = Button(master, text="Add RI-F220 registers", command=self.dev.add_modbus_reg)
        self.add_rif_btn.grid(column=2, row=2)
        
        self.add_binary_btn = Button(master, text="mb_setup RTU 9600 8N1", command=self.dev.mb_setup)
        self.add_binary_btn.grid(column=2, row=3)

        self.add_e53_btn = Button(master, text="Add Countis E53 device", command=self.dev.add_e53_dev)
        self.add_e53_btn.grid(column=1, row=4)

        self.add_rif_btn = Button(master, text="Add Countis E53 registers", command=self.dev.add_e53_reg)
        self.add_rif_btn.grid(column=2, row=4)

        self.countis_w_btn = Button(master, text="Set intervals and Sample count", command=self.open_rif_reg)
        self.countis_w_btn.grid(column=1, row=5)


    def refresh(self):
        self.rifWindow.destroy()
        self.open_rif_reg()
    
    def save_cmd(self, args):
        #retrieve current entry value
        widget = self.e.focus_get()
        #convert widget class to string
        widget_str = str(widget)
        length = len(widget_str)
        #get last character or last two characters of widget class
        last_char_widget = widget_str[length - 1 :]
        last_two_widget = widget_str[length - 2 :]
        #get inner value of entry
        widget_spec = widget.get()
        #get widget value of the measurement in the same row
        for i in range(len(self.entries)):
            for row in self.entries[i]:
                entry = row
                if entry == widget:
                    msmnt_to_change = self.entries[i][0]
                    msmnt_to_change = msmnt_to_change.get()

        if length > 18:
            if int(last_two_widget) % 3 == 0:
                self.dev.do_cmd(f"samplecount {msmnt_to_change} {widget_spec}")
            else:
                self.dev.do_cmd(f"interval {msmnt_to_change} {widget_spec}")
        elif length == 18:
            if int(last_char_widget) % 3 == 0:
                self.dev.do_cmd(f"samplecount {msmnt_to_change} {widget_spec}")
            else:
                self.dev.do_cmd(f"interval {msmnt_to_change} {widget_spec}")


    def open_rif_reg(self):
        self.rifWindow = Toplevel(self.master)
        self.rifWindow.title("Configure Device Settings")
        self.rifWindow.geometry("750x800")

        self.config_label = Label(self.rifWindow, text="Adjust measurements and hit return to confirm.")
        self.config_label.place(x=200, y=600)

        interval_min = 0

        start_time = time.monotonic()
        interval_mins = self.dev.interval_mins
        for char in interval_mins[15:]:
            if char.isdigit():
                interval_min = int(char)
        print("time taken for loop one: ", time.monotonic() - start_time)

        start_time = time.monotonic()
        # loop list of measurements and append them to list
        lst = [('Measurement','Interval (%umin)' % interval_min,'Sample Count')]
        self.mmnts = self.dev.measurements()
        print("time taken to get measurements: ", time.monotonic() - start_time)

        start_time = time.monotonic()
        self.mmnts[:]=[tuple(i) for i in self.mmnts]
        for i in range(len(self.mmnts)):
            row = list(self.mmnts[i])
            for n in range(len(row)):
                entry = row[n]
                pos = entry.find("x")
                if pos != -1:
                    row[n] = entry[0:pos]
            lst.append(row)
        print("time taken for loop two: ", time.monotonic() - start_time)

        start_time = time.monotonic()
        # find total number of rows and
        # columns in list
        total_rows = len(lst)
        total_columns = len(lst[0])
        self.entries = []
        #code for creating table
        for i in range(total_rows):
            newrow=[]
            for j in range(total_columns):
                 
                self.e = Entry(self.rifWindow, width=20, fg='blue',
                               font=('Arial',16,'bold'))
                self.e.grid(row=i, column=j)
                self.e.insert(END, lst[i][j])
                newrow.append(self.e)
                self.e.bind("<Return>", self.save_cmd)
            self.entries.append(newrow)
        print("time taken for loop three: ", time.monotonic() - start_time)
            #print(type(self.newrow[0]))
        self.save = Button(self.rifWindow, text="Save settings", command=self.dev.do_cmd("save"))
        self.save.place(x=400, y=700)

        self.refresh_page = Button(self.rifWindow, text="Refresh page", command=self.refresh)
        self.refresh_page.place(x=200, y=700)
    
    def manual_config(self):
        self.man_window = Toplevel(self.master)
        self.man_window.title("Add measurements")
        self.man_window.geometry("600x850")

        self.man_label = Label(self.man_window, text="Enter a command and hit return to send.")
        self.man_label.grid(row=2, column=2)

        self.cmd = Entry(self.man_window, width=50, fg='blue', font=('Arial',16, 'bold'))
        self.cmd.grid(row=3, column=2)
        self.cmd.bind("<Return>", self.enter_cmd)

        self.confirmed_cmd = Label(self.man_window, text="")
        self.confirmed_cmd.grid(row=4, column=2)

        self.refresh_cmd_btn = Button(self.man_window, text="New command", command=self.refresh_cmd)
        self.refresh_cmd_btn.grid(row=5, column=2)

        self.list_of_cmds = Label(self.man_window, text="", font=('Arial',11, 'bold'))
        self.list_of_cmds.grid(row=1, column=2)

        display = self.display_cmds()

    def enter_cmd(self, args):
        cmd = self.cmd.get()
        self.dev.do_cmd(cmd)

        self.confirmed_cmd.config(text="Command sent")

    def refresh_cmd(self):
        self.cmd.delete(0, END)
        self.confirmed_cmd.config(text="")
    
    def display_cmds(self):

        list_of_cmds = ""

        for cmd in cmds:
            cmd = str(cmd)
            list_of_cmds += cmd
            list_of_cmds += "\n"


        self.list_of_cmds.config(text=list_of_cmds)

cmds = [ "ios - Print all IOs.",
 "io - Get/set IO set.", 
 "sio - Enable Special IO.", 
 "count - Counts of controls.", 
 "version - Print version.", 
 "lora - Send lora message", 
 "lora_config - Set lora config", 
 "interval - Set the interval", 
 "samplecount - Set the samplecount",
 "debug - Set hex debug mask",
 "hpm - Enable/Disable HPM", 
 "mb_setup - Change Modbus comms",
 "mb_dev_add - Add modbus dev",
 "mb_reg_add - Add modbus reg",
 "mb_get_reg - Get modbus reg",
 "mb_reg_del - Delete modbus reg",
 "mb_dev_del - Delete modbus dev",
 "mb_log - Show modbus setup",
 "save - Save config",
 "measurements - Print measurements",
 "fw+ - Add chunk of new fw.",
 "fw@ - Finishing crc of new fw.",
 "reset - Reset device.",
 "cc - CC value",
 "cc_cal - Calibrate the cc",
 "w1 - Get temperature with w1",
 "timer - Test usecs timer",
 "temp - Get the temperature",
 "humi - Get the humidity",
 "dew - Get the dew temperature",
 "lora_conn - LoRa connected",
 "wipe - Factory Reset",
 "interval_min - Get/Set interval minutes",
 "bat - Get battery level.",
 "pulsecount - Show pulsecount.",
 "lw_dbg - LoraWAN Chip Debug",
 "light - Get the light in lux.",
 "sound - Get the sound in lux."]

# create root window
root = Tk()
t = Window(root)
root.geometry("650x200")
root.eval('tk::PlaceWindow . center')
root.mainloop()