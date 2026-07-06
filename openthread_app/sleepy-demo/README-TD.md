# Thread Direct Demo

The sleepy-demo-wi (WI) and sleepy-demo-wl (WL) applications demonstrate the Wake Initiator (WI) and Wake Listener (WL) functionalities of Thread Direct. Both applications are configured with RxOffWhenIdle.

## 1. Overview

- **sleepy-demo-wi node**: TBD

- **sleepy-demo-wl node**: TBD

## 2. Starting the Applications

For demonstration purposes the applications are pre-configured with Thread network dataset within the source files. In a real-life application the devices should implement and go through a commissioning process to create a network and add devices.

TBD

## 3. Thread Direct Link demonstration

TBD

### Supported Thread Direct CLI commands

Visit OpenThread CLI document to get the list of **_direct_** CLIs.

## 4. Buttons on the WI/WL

On both applications, Button 0 toggles between EM2 (sleep) and EM1 (idle) modes.
On the WI, Button 1 can be used to send a wake burst to the WL application.

## 5. Power Consumption Monitoring

Open the Energy Profiler in Simplicity Studio 5 (SSv5). In the Quick Access menu select **Start Energy Capture...** and select the WL device.

- In EM2 (sleep) mode, WI/WL current should be ... TBD.
- In EM1 (idle) mode, current is typically in the order of ... TBD.
- Further GPIO and peripheral configuration can reduce sleep current.

## 6. Sleep Callback and Interrupts

TBD

**_NOTE:_** Do not enable verbose logging because it may interfere with closely timed scheduling of transmissions and receptions.
