import serial
from binding import dev_t
import time

def virtual_osm_test():
    dev = dev_t('/tmp/osm/UART_DEBUG_slave')
    start_time = time.monotonic()
    print(dev.app_key)
    diff = time.monotonic() - start_time
    print(f"Time taken to retrieve app key: {diff}")

if __name__ == '__main__':
    virtual_osm_test()
