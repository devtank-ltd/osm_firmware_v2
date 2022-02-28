import random
import json
import string
import os

def load_config_json():
    with open("tmp/my_osm_config.json", "r+") as jfile:
        data = json.load(jfile)

def app_key_generator(size=32, chars=string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))

def dev_eui_generator(size=16, chars=string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))

def save_config():
    #Save an OSM's config as JSON
    os.system("./scripts/config_save.sh /tmp/my_osm_config.img")
    os.system("./tools/build/json_x_img /tmp/my_osm_config.img > /tmp/my_osm_config.json")

    #Generate a random dev eui and app key
    app_key = app_key_generator()
    dev_eui = dev_eui_generator()

    data = load_config_json()
    data["lw_dev_eui"] = dev_eui
    data["lw_app_key"] = app_key
    for item in data['measurements'][item]['interval']:
        if data['measurements'][item]['interval'] == 1:
            data['measurements'][item]['interval'] = 0
    with open("tmp/my_osm_config.json", 'w') as nfile:
        json.dump(data, nfile, indent=2)


def set_pulse_count_special():
    data = load_config_json()
    for item in data['ios']:
        if data['ios'][item] == 4:
            data['ios'][item]['pull'] = 'NONE'
            data['ios'][item]['direction'] = 'IN'
            data['ios'][item]['use_special'] = True
            with open("tmp/my_osm_config.json", 'w') as nfile:
                json.dump(data, nfile, indent=2)

def set_onewire_special():
    data = load_config_json()
    for item in data['ios']:
        if data['ios'][item] == 4:
            data['ios'][item]['pull'] = 'NONE'
            data['ios'][item]['direction'] = 'IN'
            data['ios'][item]['use_special'] = True
            with open("tmp/my_osm_config.json", 'w') as nfile:
                json.dump(data, nfile, indent=2)

def restore_config():
    data = load_config_json()
    for item in data['measurements'][item]['interval']:
        if data['measurements'][item]['interval'] == 0:
            data['measurements'][item]['interval'] = 1
            with open("tmp/my_osm_config.json", 'w') as nfile:
                json.dump(data, nfile, indent=2)
    #Write out the JSON to the OSM
    os.system("./tools/build/json_x_img /tmp/my_osm_config.img < /tmp/my_osm_config.json")
    os.system("./scripts/config_load.sh /tmp/my_osm_config.img")
        
if __name__ == "__main__":
    save_config()
