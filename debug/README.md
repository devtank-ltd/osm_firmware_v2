```
 / ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ \
|  /~~\                                                  /~~\  |
|\ \   |      ________  _________   ____      ____      |   / /|
| \   /|     / ____/  |/  / ____/  /  _/___  / __/___   |\   / |
|  ~~  |    / __/ / /|_/ / /       / // __ \/ /_/ __ \  |  ~~  |
|      |   / /___/ /  / / /___   _/ // / / / __/ /_/ /  |      |
|      |  /_____/_/  /_/\____/  /___/_/ /_/_/  \____/   |      |
|      |                                                |      |
|      |                                                |      |
 \     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|     /
  \   /                                                  \   /
   ~~~                                                    ~~~
```

1.  First use screen (`screen /dev/ttyUSB0` likely) to make sure the
    IOs are set up correctly, ensuring the IO for w1 and pulsecounting
    is set up. Save and perhaps reset.

2.  Ensure that the I2C is working (if the board has just been
    reprogrammed if will require a reboot (either reset or power
    button).

3.  Add/check the Modbus measurements wanting to log with `mb_log`.

4.  Check it has successfully connected to the LW network, this should
    be fine if done in debug mode but would 'taint' the results.

5.  Enable debug mode with "debug_mode".

6.  CLOSE the serial connection (screen).

7.  Start the docker with `docker start emc_svr`.

8.  Start the Modbus server with `python3 fake_meter.py /dev/ttyUSB1`,
    maybe check beforehand which tty the RS485 adaptor and the OSM is
    on.

9.  Setup the CAN network with `./can/setup_real_canbus.sh`.

10. Start the logger with `python3 debug_influx.py`.

Notes:
=====

 ·  Unfortunately I didnt get around to adding a parser for the logging
    script and so changing the tty the OSM is on is done by editing the
    file and changing the address of the influx database is again done
    by editing the file.

 ·  Logins are generally admin admin. This goes for the grafana as well,
    (you can press skip when it asks for you to change it).

 ·  Perhaps calibrate current clamp beforehand?

 ·  The graphs on grafana have been set up with a minimum interval of
    2 seconds and to never join NULL values, this is because the OSM
    sends messages every other second and if there aren't messages in an
    interval then we want to see a gap (could be error?).
