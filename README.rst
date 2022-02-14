Shortcut-Remote-Demo
####################

Overview
********
This demo shows how to setup an nRF52 kit as a streaming remote for controlling various actions on your PC. 
It uses a USB receiver on the PC side to receive a signal over Bluetooth (hid_receiver project) and send it to the PC over USB, in order to support PC's without native Bluetooth support. 
The remote project sets up an nRF52DK as a remote, and uses the buttons on the DK to send various commands to the PC through the hid_receiver:

Button 1 - Volume down

Button 2 - Volume up

Button 3 - Send a keyboard key (starting with 'a', incrementing through the alphabet for each press)

Button 4 - Shift + Ctrl + m (shortcut for muting microphone in Teams)

Requirements
************
Tested in nRF Connect SDK v1.8.0

Supported (tested) boards:

*remote*

- nrf52dk_nrf52832

- nrf52840dk_nrf52840

*hid_receiver*

- nrf52840dk_nrf52840

- nrf52840dongle_nrf52840 (PCA10059)

TODO
****
There are some planned features not currently implemented:

- Add the possibility to change the command mapping without having to change the code. The plan is to use a separate Bluetooth connection to send configuration data, and have it stored in internal flash so that it is persistent. 

- Add an option of having the remote communicate directly with the PC over HID over GATT, in order to remove the need for a dongle on PC's with native Bluetooth support

- Add support for a custom board with more buttons
