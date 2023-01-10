# esp32-crosstrainer
Aim of this project is to monitor usage of a specific crosstrainer (SPORTOP E820), record the data and upload to a Google Script which in turn updates a Google Sheet with the latest values.

**This is the first time I'm sharing my code so there might be some issues here.**

## Hardware

Custom PCB was designed for this project. It's mainly an ESP32 with a voltage regulator and a suitable connector so that the device can be attached in line with the original circuitry of the crosstrainer. Since the crosstrainer can be battery powered, there is a circuit to measure the battery voltage. Additionally there is a custom programming connector, some buttons, testpoints and an activity LED.

## Software

Original idea was to minimize the current consumption of the device to allow it to be battery powered. The ULP core is used to collect the usage data and the main core is only woken up to upload the data to Google Apps Script. In retrospect, this isn't really necessary when compared to average current consumption of the crosstrainer itself.

### ULP

The ULP is programmed to detect falling edges on a pin that is connected to a reed switch in the crosstrainer. After that the program goes into active mode and counts the edges, tracks time and measures the electronically selected load (difficulty) of the crosstrainer. The main CPU is woken up after there is no falling edges for a preset amount of time.

### ESP32 Main CPU

The main CPU simply reads the values collected by the ULP code, averages the load and uploads all the data to a preset Google Apps Script. If the upload doesn't complete in a specific time, the data is stored in RTC memory and upload is retried next time the main CPU wakes up.

### Google Apps Script

The script adjusts the uploaded values to a more readable format and creates a new line in a table. Energy used for the excercise is also calculated from the measured values (using some multiplier to match the values the crosstrainer itself displays). The graphs in the sheet update automatically with the new data. An additional feature is to send a warning email to the user in case the battery voltage is getting low.

## Issues

- the data is stored internally without a timestamp, so the timestamp will be wrong if the first upload fails
- battery measurement from the ULP core is not finished
- temperature measurement with ULP core didn't work for me at all for some reason

## License
[MIT](https://choosealicense.com/licenses/mit/)
