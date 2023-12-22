# Config GUI Web

Use this application to open a GUI to enable you to configure an OSM with various settings.

## Run Application

The main page to connect to the gui is found in app/config_gui.html.

Open this in a local web server to run the application, there are several ways to do this
including installing a Live Server extension in VSCode.

Other ways include:
1. Installing nodejs `sudo apt install nodejs`

2. Navigate to the location of `config_gui.html`

3. Run a http server with `npm install http-server -g`

4. Go to localhost:8080 in your browser.

Once you have reached the home page, connect an OSM via micro USB, press connect and select the device.

## File Structure

The binding between the OSM and the user is found in app/modules/backend/binding.js.

The code for the front end of the application is found in app/modules/gui.

The HTML that these scripts use is found in app/modules/gui/html.

The CSS is found in app/modules/styles.
