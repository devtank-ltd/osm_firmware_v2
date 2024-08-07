#! /usr/bin/python3

import os
import sys
import time
import logging
import argparse
import subprocess

from binding import dev_t

import json

sys.path.append(os.path.join(os.path.dirname(__file__), "../tools/json_config_tool/"))

import json_config


class virtual_osm:
    def __init__(self, logger, base_dir, name, firmware_path, hw_id):
        self._logger = logger
        self.name = name
        self._firmware_path = firmware_path
        self._loc = f"{base_dir}/{name}"
        self._hw_id = hw_id
        cmd = [f"OSM_HW_ID={self._hw_id} OSM_LOC={self._loc} {self._firmware_path}"]
        self._logger.debug(f"Starting Virtual OSM {name} : {cmd[0]}")
        self._vosm_subp = subprocess.Popen(cmd, shell=True)
        self._binding = None

    def pid(self):
        return self._vosm_subp.pid

    def is_alive(self):
        return self._vosm_subp.poll() is None

    def close(self):
        if self.is_alive():
            self._vosm_subp.terminate()

    def get_tty(self):
        return f"{self._loc}/UART_DEBUG_slave"

    def get_binding(self):
        if not self._binding:
            tty = self.get_tty()
            if not os.path.exists(tty):
                return None
            self._logger.debug(f"Virtual OSM {self.name} {tty} ready")
            self._binding = dev_t(tty)
        return self._binding

    def load_config(self, config_dict):
        self._logger.debug("Loading configuration")
        binding = self.get_binding()
        if not binding:
            return False
        json_obj = json_config.create_json_dev(binding)
        json_obj.from_dict(config_dict)

    @property
    def pid(self):
        return self._vosm_subp.pid


def main(args):
    if args.verbose:
        logging.basicConfig(level=logging.INFO)

    if args.debug:
        logging.basicConfig(level=logging.DEBUG)

    logger = logging.getLogger(__file__)

    logger.debug(f'Spawning network from config "{args.config}"')

    network_config_path = args.config
    with open(network_config_path) as f:
        config = json.loads(f.read())

    base_dir = os.path.join(os.path.dirname(network_config_path), config['base_dir'])
    if not os.path.exists(base_dir):
        os.mkdir(base_dir)

    fake_osm_configs = config['fake_osms']
    fake_osms = {}
    logger.debug(f'Starting OSM fakes')
    for fake_osm_name, fake_osm_config in fake_osm_configs.items():
        logger.debug("="*72)
        firmware_path = os.path.join(os.path.dirname(network_config_path), fake_osm_config['firmware'])
        hw_id = fake_osm_config["hw_id"]
        fake_osm = virtual_osm(logger, base_dir, fake_osm_name, firmware_path, hw_id)
        dev_binding = None
        while not dev_binding:
            try:
                dev_binding = fake_osm.get_binding()
                if not dev_binding and not fake_osm.is_alive():
                    logger.error(f"Virtual OSM {fake_osm_name} died.")
                    break
            except Exception as e:
                if fake_osm.is_alive():
                    logger.debug(f"Waiting on Virtual OSM...(PID:{fake_osm.pid})")
                    time.sleep(0.25)
                else:
                    logger.error(f"Virtual OSM {fake_osm_name} died.")
                    break
        if fake_osm.is_alive():
            config_file = os.path.join(os.path.dirname(network_config_path), fake_osm_config['config_file'])
            with open(config_file) as f:
                fake_osm_config_data = json.loads(f.read())
            overloads = fake_osm_config.get('overloads', {})
            overloads.update({"name": fake_osm_name})
            if overloads:
                for key, value in overloads.items():
                    fake_osm_config_data[key] = value
            fake_osm.load_config(fake_osm_config_data)
            fake_osms[fake_osm_name] = fake_osm

    logger.debug("="*72)
    if len(fake_osms) == len(fake_osm_configs):
        logger.debug("Waiting for OSMs to exit")
    else:
        logger.error("Unable to start OSM network.")
        logger.debug("Closing")
        for fake_osm in fake_osms.values():
            fake_osm.close()
    while True:
        try:
            os.waitid(os.P_ALL, 0, os.WEXITED)
            break
        except KeyboardInterrupt:
            logger.error("Keyboard interrupt.")
        except ChildProcessError:
            break
    logger.debug("All OSMs closed.")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Load a fake OSM network.',
        epilog='Run this program with the config as an argument e.g. python spawn_network.py spawn_network_example.json'
    )
    parser.add_argument('-v','--verbose', help='Info log information', action='store_true')
    parser.add_argument('-d','--debug', help='Debug log information', action='store_true')
    parser.add_argument('config')
    args = parser.parse_args()
    sys.exit(main(args))
