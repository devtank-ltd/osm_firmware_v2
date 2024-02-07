#!/usr/bin/env python3

import http.server
import socketserver
import json
import random
from threading import Thread
import signal
import time
import os
import subprocess
import string
import atexit

all_processes = {}
all_threads = []

class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.path = './config_gui.html'
        return http.server.SimpleHTTPRequestHandler.do_GET(self)

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        received_data = json.loads(post_data.decode('utf-8'))
        if received_data["cmd"] == 'Close':
            p = received_data["port"]
            process = all_processes[p]
            fw_pid = int(process.pid)
            bash_pid = fw_pid + 1
            os.kill(bash_pid, signal.SIGINT)
            os.kill(fw_pid, signal.SIGINT)
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            response = json.dumps({'Closed': True})
            self.wfile.write(response.encode('utf-8'))
            return response

        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()

        self.port = self.gen_random_port()
        loc = self.random_str()
        location = f"/tmp/osm_{loc}/"
        websocket = f"ws://localhost:{self.port}/websocket"

        osm_bin_exists = self.find_linux_binary()
        if not osm_bin_exists:
            linux_osm = self.generate_linux_binary()

        if linux_osm == 0:
            cmd = [f"DEBUG=1 USE_PORT={self.port} USE_WS=1 LOC={location} ../../../build/penguin/firmware.elf"]
            response = json.dumps({'location': location, "websocket": websocket, "port": self.port})
            self.wfile.write(response.encode('utf-8'))
            self.spawn_virtual_osm(cmd, self.port)
            time.sleep(2)
            return response

    def gen_random_port(self):
        port = random.randint(1096, 65535)
        return port

    def random_str(self):
        length = 20
        letters = string.ascii_lowercase
        return ''.join(random.choice(letters) for i in range(length))

    def spawn_virtual_osm(self, cmd, port):
        self.vosm_subp = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        all_processes[port] = self.vosm_subp
        return self.vosm_subp

    def find_linux_binary(self):
        output = '../../../build/penguin/firmware.elf'
        try:
            sub = subprocess.check_output(f'ls {output}')
            return True
        except FileNotFoundError:
            return False

    def generate_linux_binary(self):
        sub = subprocess.run('cd ../../.. && make penguin && cd -', stdout=subprocess.PIPE, shell=True)
        return sub.returncode

@atexit.register
def cleanup():
    timeout_sec = 5
    for key, p in all_processes.items():
        p_sec = 0
        for second in range(timeout_sec):
            if p.poll() == None:
                time.sleep(1)
                p_sec += 1
        if p_sec >= timeout_sec:
            p.kill()
            global stop_threads
            stop_threads = True
            for thr in all_threads:
                thr.join()


def run_forever():
    handler_object = MyHttpRequestHandler
    PORT = 8000
    my_server = socketserver.TCPServer(("", PORT), handler_object)
    while True:
        my_server.serve_forever()
        if stop_threads:
            break

stop_threads = False
t = Thread(target=run_forever)
t.start()
all_threads.append(t)
