AD5941 ESP32-C3 Multi-Channel DPV Electrochemical Monitor

1. Project overview

This project is a custom differential pulse voltammetry, DPV, electrochemical monitor based on AD5941 and ESP32-C3. The goal is to build a low-cost and compact prototype board for biochemical laboratory testing. The AD5941 is used for electrochemical excitation, current measurement, and ADC conversion. The ESP32-C3 runs the Arduino firmware and sends DPV data to a PC upper-computer program through serial communication.

The hardware supports three working electrodes, WE1, WE2, and WE3, switched by an ADG704 analog switch. Each WE channel has been tested separately. The current recommended version uses serial communication. WiFi web display and TF card logging were tested earlier, but the TF card module caused interference on the shared SPI bus and affected AD5941 communication. Therefore, WiFi and TF card functions are disabled in the current stable laboratory version.

2. Current features

The PC upper-computer program can connect to ESP32-C3 through a serial port, configure DPV parameters, configure system parameters, start or stop measurement, display the curve in real time, and save data. The firmware uploads DPV data including working electrode channel, potential, current, differential current, sampling time, and valid flag.

WE1, WE2, and WE3 can be selected. Formal DPV measurement should use the internal RTIA of AD5941. The currently recommended internal RTIA value is 10000 ohm. The recommended RCAL value for the current board is 195 ohm, based on dummy-load calibration.

3. Test results

The board has been validated with resistor dummy loads. With a 100 k ohm resistor and a 0 to 200 mV scan, the measured current was close to the theoretical value. This verifies the voltage output, current measurement, and serial data upload path.

The board has also measured a visible DPV peak in potassium ferricyanide solution. The curve still has ripple and the peak amplitude does not fully match a commercial electrochemical workstation, but the prototype can already detect the electrochemical peak.

DPV result from this AD5941 board in potassium ferricyanide solution:

![AD5941 board DPV result](img/result1.png)

DPV result from a commercial electrochemical workstation under similar potassium ferricyanide testing conditions:

![Commercial electrochemical workstation result](img/comm_stat.png)

The comparison shows that the custom board can detect the peak, but the curve is noisier and the peak amplitude is not yet fully matched. Possible reasons include unmatched DPV parameters, electrode condition, solution condition, analog front-end noise, sampling timing, and RTIA settings.

4. Recommended test parameters

Recommended system parameters: RCAL = 195 ohm, RTIA source = internal RTIA, internal RTIA = 10000 ohm, ADC Ref = 1.8162 V, ADC PGA = 1.5, VZERO = 1100 mV, Timeout = 1000 to 2000 ms.

Recommended dummy-load calibration parameters: Start = 0 mV, End = 200 mV, Step = 20 mV, Pulse Amp = 0 mV, Quiet Time = 2000 ms, Settle Time = 100 to 200 ms.

Recommended potassium ferricyanide DPV parameters should be matched to the commercial workstation as closely as possible. A practical starting point is Start = -300 mV, End = 300 mV, Step = 4 or 5 mV, Pulse Amp = 50 mV, Pulse Time = 60 ms, Quiet Time = 2000 ms, and Settle Time = 200 ms.

If the curve is unstable, first reduce the scan rate, increase Settle Time, check electrode contact, remove bubbles, and wait for the solution to stabilize. Avoid changing too many parameters at the same time.

5. Current limitations and notes

This project is not yet a replacement for a commercial electrochemical workstation. It is a working prototype that can detect DPV peaks and still needs biochemical calibration and repeatability testing.

Important notes: 1) TF card logging is not recommended because the tested TF card module affected AD5941 SPI communication and sometimes caused AD5941 ID read failure. 2) WiFi web monitoring is also not recommended for the current laboratory version, and serial communication should be used instead. 3) External RTIA-related resistors are currently kept for hardware diagnosis only and are not recommended as the formal DPV RTIA path. 4) DPV results are sensitive to electrode condition, solution concentration, bubbles, connection quality, quiet time, pulse amplitude, and sampling timing.

6. Future work

Future work includes building calibration curves with different concentrations of potassium ferricyanide, comparing peak current and peak potential against a commercial workstation, improving sampling timing and noise handling, testing repeatability of WE1, WE2, and WE3, and revisiting TF card or WiFi only after the hardware connection issue is solved.

7. Repository contents

This repository contains the Arduino firmware, AD5941 and AD5940 support files, ESP32-C3 and AD5941 pin configuration, DPV parameter notes, board function notes, schematic files, potassium ferricyanide test image, and commercial workstation reference image.
