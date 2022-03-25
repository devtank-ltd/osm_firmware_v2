from tkinter import *
from tkinter.ttk import Combobox
import serial.tools.list_ports
import serial
import re
import io

class Window(Frame):
    def __init__(self, master=None):
        Frame.__init__(self, master)
        self.master = master

        usb_label = Label(master, text="Select a device", pady=1)
        usb_label.pack()

        returned_ports = []
        active_ports = serial.tools.list_ports.comports(include_links=True)
        for item in active_ports:
            returned_ports.append(item.device)
            
        self.dropdown = Combobox(master, values = returned_ports)
        for item in active_ports:
            if "tty" in item.device:
                self.dropdown.set(item.device)
        self.dropdown.pack(padx = 5, pady = 5)

        self.conf_btn = Button(master, text="Confirm device", command=self.query_value)
        self.conf_btn.pack(padx=3, pady=3)

        self.wipe_dev = Button(master, text="Wipe device", command = self.wipe_clean)
        self.wipe_dev.pack(padx=3, pady=3)

        self.msm_label = Label(master, text="Select measurements you want to add to device", pady=1)
        self.msm_label.pack(padx=3, pady=3)

        self.e53_btn = Button(master, text="Add Countis E53 Dev and Registers", command=self.add_e_template)
        self.e53_btn.pack(padx=3, pady=3)

        self.rif_btn = Button(master, text="Add Rayleigh RI F Dev and Registers", command=self.add_r_template)
        self.rif_btn.pack(padx=3, pady=3)

        #self.add_e53_only = Button(master, text="Add E53 dev only", command=self.add_countis_only)
        #self.add_e53_only.pack(padx=3, pady=3)
#
        #self.add_rif_only = Button(master, text="Add RI F dev only", command=self.add_dev_only)
        #self.add_rif_only.pack(padx=3, pady=3)
#
        #self.pow_f_btn = Button(master, text="Add Power Factor", command=self.write_PF)
        #self.pow_f_btn.pack(padx=3, pady=3)
#
        #self.volt_btn = Button(master, text="Add Voltage", command=self.write_V)
        #self.volt_btn.pack(padx=3, pady=3)
#
        #self.amps_btn = Button(master, text="Add Amps", command=self.write_A)
        #self.amps_btn.pack(padx=3, pady=3)
#
        #self.imp_btn = Button(master, text="Add Import Energy", command=self.write_imp)
        #self.imp_btn.pack(padx=3, pady=3)
#
        #self.reg_added = Label(master, text="", pady=1)
        #self.reg_added.pack()


    def query_value(self):
        print("Selected Option: {}".format(self.dropdown.get()))
        self.device_to_use = self.dropdown.get()
        with serial.Serial(self.device_to_use, timeout=1) as ser:
            self.sio = io.TextIOWrapper(io.BufferedRWPair(ser, ser))
    
    def wipe_clean(self):
            self.sio.write("wipe")
    
    def add_e_template(self):
        self.sio.write("mb_dev_add 5 E53")
        self.sio.write("mb_reg_add 5 0xc56e 3 U32 PF")
        self.sio.write("mb_reg_add 5 0xc552 3 U32 cVP1")
        self.sio.write("mb_reg_add 5 0xc554 3 U32 cVP2")
        self.sio.write("mb_reg_add 5 0xc556 3 U32 cVP3")
        self.sio.write("mb_reg_add 5 0xc560 3 U32 mAP1")
        self.sio.write("mb_reg_add 5 0xc562 3 U32 mAP2")
        self.sio.write("mb_reg_add 5 0xc564 3 U32 mAP3")
        self.sio.write("mb_reg_add 5 0xc652 3 U32 ImEn")
    
    def add_r_template(self):
        self.sio.write("mb_dev_add 1 RIF")
        self.sio.write("mb_reg_add 1 0x36 4 F PF")
        self.sio.write("mb_reg_add 1 0x00 4 F VP1")
        self.sio.write("mb_reg_add 1 0x02 4 F VP2")
        self.sio.write("mb_reg_add 1 0x04 4 F VP3")
        self.sio.write("mb_reg_add 1 0x10 4 F AP1")
        self.sio.write("mb_reg_add 1 0x12 4 F AP2")
        self.sio.write("mb_reg_add 1 0x14 4 F AP3")
        self.sio.write("mb_reg_add 1 0x60 4 F Imp")
    
    def add_countis_only(self):
        self.sio.write("mb_dev_add 5 E53")

    def add_dev_only(self):
        self.sio.write("mb_dev_add 1 RIF")
    
    def write_PF(self):
        self.sio.write("mb_reg_add 5 0xc56e 3 U32 PF")
    
    def write_V(self):
        self.sio.write("mb_reg_add 5 0xc552 3 U32 cVP1")
        self.sio.write("mb_reg_add 5 0xc554 3 U32 cVP2")
        self.sio.write("mb_reg_add 5 0xc556 3 U32 cVP3")
    
    def write_A(self):
        self.sio.write("mb_reg_add 5 0xc560 3 U32 mAP1")
        self.sio.write("mb_reg_add 5 0xc562 3 U32 mAP2")
        self.sio.write("mb_reg_add 5 0xc564 3 U32 mAP3")
    
    def write_imp(self):
        self.sio.write("mb_reg_add 5 0xc652 3 U32 ImEn")
      
    
    def write(self, msg):
        self.sio.write("%s\n"% msg)
        self.sio.flush()

    def read(self):
        try:
            msg = self.sio.readline().strip("\r\n")
        except UnicodeDecodeError:
            return None
        if msg == '':
            return None
        return msg

    def readlines(self):
        msg = self.read()
        msgs = []
        while msg != None:
            msgs.append(msg)
            msg = self.read()
        return msgs

        





root = Tk()
app = Window(root)
root.wm_title("Configure Device")
root.geometry("740x400")
root.mainloop()