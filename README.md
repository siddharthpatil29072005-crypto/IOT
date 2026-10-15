# Blown Film Extrusion Bubble Diameter Measurement

Arduino Uno Wokwi simulation for measuring blown-film bubble diameter with two opposite HC-SR04 sensors.

## Measurement model

```text
D = sensorSeparationCm (default: 80 cm)
Bubble diameter = D - (left distance + right distance)
Target diameter = 50 cm
Allowed tolerance = +/-2 cm
```

The LCD shows the calculated diameter and either `OK` or `ALARM`. The green LED (D3) indicates an in-tolerance measurement; the red LED (D4) indicates an alarm or invalid sensor measurement.

Sensor-driven measurement is enabled by default. Move either Wokwi HC-SR04 distance control to change `d1` or `d2`. Change `sensorSeparationCm` in `IOT.ino` when the physical distance `D` between the sensors changes. The green LED blinks while the diameter is within `48-52 cm`; the red LED blinks outside that band.

## Files

- `IOT.ino`: Non-blocking sensor state machine, diameter calculation, LCD output, and LED logic.
- `diagram.json`: Complete Wokwi circuit and pin wiring.
- `wokwi.toml`: Wokwi firmware/ELF paths.

## Quick start in VS Code

1. Install the Arduino extension (`Microsoft Arduino`, `vsciot-vscode.vscode-arduino`) or install Arduino CLI.
2. Install the AVR core if using Arduino CLI:

   ```powershell
   arduino-cli core update-index
   arduino-cli core install arduino:avr
   arduino-cli lib install LiquidCrystal_I2C
   ```

3. Compile from this project directory:

   ```powershell
   arduino-cli compile --fqbn arduino:avr:uno --output-dir build --export-binaries .
   ```

   This creates `build/IOT.ino.hex` and `build/IOT.ino.elf`, matching `wokwi.toml` when the workspace folder is named `IOT`.
4. Install the Wokwi Simulator extension (`wokwi.wokwi-vscode`).
5. Open `diagram.json`, then run **Wokwi: Start Simulator** from the Command Palette. If the extension asks for a Wokwi verification code, follow its sign-in prompt.

### Arduino extension alternative

Select the Arduino Uno board and port from the Arduino extension, then run **Arduino: Verify**. The extension can also upload to physical hardware; Wokwi uses the compiled artifacts referenced in `wokwi.toml`.

## Interacting with the simulation

Each HC-SR04 has a Wokwi distance control/slider. Move the left and right sensor sliders independently while the simulation is running:

- Set both distances near `15 cm` to produce approximately `50 cm` diameter and turn the green LED on.
- Change either distance so the calculated diameter leaves `48-52 cm` to turn the red LED on and display `ALARM`.
- Use an invalid/out-of-range sensor condition to verify the `Sensor fault` display and red alarm.

The wiring is:

| Device | Arduino Uno |
| --- | --- |
| Left HC-SR04 TRIG / ECHO | D9 / D8 |
| Right HC-SR04 TRIG / ECHO | D7 / D6 |
| LCD1602 SDA / SCL | A4 / A5 |
| Green LED | D3 through 220 ohm resistor |
| Red LED | D4 through 220 ohm resistor |
| Left / right film-wall servos | D5 / D10 |
