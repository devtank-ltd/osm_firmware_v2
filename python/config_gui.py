from asyncio import base_tasks
from cgitb import text
from logging import exception
from shutil import ExecError
from tkinter import *
from tkinter.ttk import Combobox
from click import command
import serial.tools.list_ports
import serial
import re
import io
import random
import string

from sqlalchemy import column
import binding

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

        self.debug_mb_btn = Button(master, text="Set debug to modbus", command=self.dev.modbus_debug)
        self.debug_mb_btn.grid(column=0, row=3)

        self.mb_setup_btn = Button(master, text="See modbus setup", command=self.dev.see_mb_setup)
        self.mb_setup_btn.grid(column=1, row=2)

        self.add_dev_btn = Button(master, text="Add RIF device", command=self.dev.add_mb_dev)
        self.add_dev_btn.grid(column=1, row=3)

        self.add_rif_btn = Button(master, text="Add RIF registers", command=self.dev.add_modbus_reg)
        self.add_rif_btn.grid(column=2, row=2)
        
        self.add_binary_btn = Button(master, text="Add Binary setting", command=self.dev.do_cmd("mb_setup BIN 9600 8N1"))
        self.add_binary_btn.grid(column=2, row=3)

        self.countis_w_btn = Button(master, text="Set intervals and Sample count", command=self.open_rif_reg)
        self.countis_w_btn.grid(column=1, row=4)   


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
        self.rifWindow.geometry("800x800")

        interval_min = 0

        interval_mins = self.dev.interval_mins
        for char in interval_mins[15:]:
            if char.isdigit():
                interval_min = int(char)
  
        # loop list of measurements and append them to list
        lst = [('Measurement','Interval (%umin)' % interval_min,'Sample Count')]
        self.mmnts = self.dev.measurements()
        self.mmnts[:]=[tuple(i) for i in self.mmnts]
        for i in range(len(self.mmnts)):
            row = list(self.mmnts[i])
            for n in range(len(row)):
                entry = row[n]
                pos = entry.find("x")
                if pos != -1:
                    row[n] = entry[0:pos]
            lst.append(row)

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
            #print(type(self.newrow[0]))
        self.save = Button(self.rifWindow, text="Save settings", command=self.save_cmd)
        self.save.place(x=400, y=600)

        self.refresh_page = Button(self.rifWindow, text="Refresh page", command=self.refresh)
        self.refresh_page.place(x=200, y=600)


# create root window
root = Tk()
t = Window(root)
root.geometry("600x200")
root.eval('tk::PlaceWindow . center')
root.mainloop()