from distutils.core import setup
import py2exe
#Run this program on a windows operating system to create a config gui executable e.g python windows_setup.py py2exe

# Dependencies (add any additional dependencies here)
dependencies = [
    "binding",
    "modbus_db",
    "modbus_funcs"
]

# Data files
data_files = [
    ("osm_pictures", ["osm_pictures/icons-together.png",
                      "osm_pictures/OSM+Powered.png",
                      "osm_pictures/Lora-Rev-C.png",
                      "osm_pictures/shuffle.png",
                      "osm_pictures/graph.png",
                      "osm_pictures/parameters.png",
                      "osm_pictures/opensource-nb.png",
                      "osm_pictures/leaves.jpg"]),
    ("config_database", ["config_database/modbus_templates.db"]),
]


# Create the setup configuration
setup(
    py_modules = [],
    windows=["config_gui.py"],  # Main script to be executed
    options={
        "py2exe": {
            "includes": dependencies,  # Include dependencies
            "bundle_files": 1  # Bundle files into the executable
        }
    },
    data_files=data_files,  # Include data files
)
