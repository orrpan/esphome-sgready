```yaml
# example configuration:

switch:
  - platform: sgready_switch
    name: SGReady switch
```

# OUT
## Switch
* On/Off
* Disable 3
* Disable 4
* SGReady A
* SGReady B

## Sensors
* Mode
* Time Blocked

## Input number
* Minimum temp

## Buttons
* Mode 1
* Mode 2
* Mode 3
* Mode 4

# In
## Home assistant
* Elprissnitt
* Relay 1
* Relay 2

## esphome
* Temperatur

# Rules

* Minumum between change 10 minutes
  * VERY LOW
    * Allow orderd mode (Fall through to NORMAL_OPERATION)
  * LOW
    * Allow encourage mode (Fall through to NORMAL_OPERATION)
  * NORMAL
  * HIGH
    * \< 3 times BLOCKED_OPERATION (Fall through to NORMAL_OPERATION)
    * \< 2 hours BLOCKED_OPERATION (Fall through to NORMAL_OPERATION)
    * \> minimum temperature (Fall through to NORMAL_OPERATION)
