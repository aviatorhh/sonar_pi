# sonar_pi
Plugin for OpenCPN that shows sonar sensor data.
Data coming from an open_echo (https://github.com/Neumi/open_echo) device is displayed on a dockable window inside OpenCPN. It also provides the depth via NMEA sentence provisioning.
Data coming from an open_echo (https://github.com/Neumi/open_echo) device is displayed on a dockable window inside OpenCPN. It also provides the depth via NMEA sentence provisioning.

* based on wxWidgets 3.2.2.1 - other versions not tested
* needs OpenGL (glut.h)
* based on API 1.18

<img width="833" height="1137" alt="Screenshot 2026-01-13 at 10 40 37" src="https://github.com/user-attachments/assets/677b59d5-d216-49a4-92c6-dc5634a0bd86" />

Though finally not ready for implementation yet, first tests are running successfully.

More TODOs

* Change GUI to reflect new architecture (front end and preferences)
* Make configurable to be used with open_echo directly or with own device design
* Provide harware setup and a easy to implement solution ("plug'n'play")
* Make use of a more current OpenGL architecture (makes multi OS compatibilty harder)