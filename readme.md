# Architecture

- led devices (many)  [MQTT]  
- clients             [MQTT]  
- api [MQTT <--> REST]  
    - client controllers [REST] 
 
## Database design

### Device records (stored in REDIS as JSON strings under keys: "device:[device_id]")
1. state: "on|off|blinking"
2. blinkInfinite?: boolean  - null if state != blinking
3. blinkDelay?: number      - null if state != blinking
5. blinkCount?: number      - null if state != blinking OR blinkInfinite == true 

### Group records (stored in REDIS as JSON strings under keys: "group:[group_id]")
1. id: incrementing number
2. leds: [] // list of led ids in this group

### Users table (not implemented, but would be JSON strings under "user:[user_id]")
//1. uuid: uuid
2. username: string
3. password: string (protected)
4. devices: list of device ids 

## MQTT topic design

### leds/{uuid}

#### /set_state_simple
Basic control over the LED  
control/api --> led  
LED replies with /state  
Payload: 
```
{  
    state: "on|off|toggle"
}  
```
#### /set_state
Full control over the LED  
control/api --> led  
Payload: 
```
{  
    state: "on|off|blinking"  
    if state == blinking: (otherwise unneeded)  
        infinite: boolean,  
        blinkCount: number,
        delay: number  
}
```
#### /leds/state
A message containing the current state of the LED, sent after a command and received by the server to update the current data stored.  
led --> all  
Payload: 
```
{  
    id: uuid
    state: "on|off|blinking"  
    if state == blinking:  
        infinite: boolean,  
        blinkCount: number,
        delay: number  
}
```
### leds/manage

#### /add
led --> all  
Creates a new LED device  
This is used to keep Redis updated of the current devices - sent by LEDs on startup
Payload: 
```
{
    uuid: uuid
    description: string
}
```
#### /remove
led --> all  
Removes an LED device  
This is used to keep Redis updated of the current devices - sent by LEDs as their last will and testament
Payload: 
```
{
    uuid: uuid
}
```
## API design (/api)

### LED Control (.../leds/)
#### get state (.../{uuid}) - GET
Payload: 
```
{}
```
Return: 
```
{
    success: boolean,
    newState: "on|off"
}
```

#### set state (.../{uuid}) - PUT
Payload: 
```
{
    state: "on"|"off|toggle"
}
```
Return: 
```
{
    success: boolean,
    newState: "on|off"
}
```

#### blink (.../{uuid}/blink) - POST
Payload: 
```
{
    infinite: boolean,
    blinkCount (# of on-delay-off-delay cycles): number,
    delay (ms between state changes): number
}
```
Return: 
```
{
    success: boolean,
    newState: "blinking",
    infinite: boolean,
    blinkCount: number,
    delay: number
}
```
#### get state (.../{uuid}) - GET
Payload: {}  
Return: 
```
{
    state: "on|off|blinking"
    if state == blinking:
        infinite: boolean,
        blinkCount: number,
        delay: number
}
```
##### set state for many LEDs (.../) - PUT
Set many LEDS at once with data passed as a JSON array as following:
Request:
```
    [{
        "id": "{id}",
        "state": {
            "state": "on|off|toggle|blink"
            if state is blinking:
                "infinite": boolean,
                "blinkCount": number,
                "delay": number
                }
    }]
```
Return:
```
    [{push
        "id": "{id}",
        "state": {
            "state": "on|off|blink"
            if state is blinking:
                "infinite": boolean,
                "blinkCount": number,
                "delay": number
                }
    }]
```
### LED groups (.../groups)

#### create a group (../) - POST
Payload: 
```
{
    leds: [{ids...}]
    name: string
    description: string
}
```
Return: 
```
{
    leds: [{ids...}]
    name: string
    description: string
    id: string // id of newly created group
}
```
#### get a group (.../{group_id}) - GET
Payload: {}
Return: 
```
{
    leds: [{led_ids}...]
    name: string
    description: string
}
```

#### list all LEDs in a group (.../{group_id}/leds) - GET
Payload: {}
Return: 
```[
    {
        id: string,
        state: "on|off|blinking"
        w/ blinking details
    }...
]
```

#### edit a group - i.e. add/remove LEDs (.../{group_id}) - PATCH
Does not use PUT, as only the led list changes
Payload: 
```
{
    add: []     // list of LED ids
    remove: []  //list of LED ids
}
```
Return: 
```
{
    id: string // of group
    leds: [] // list of LED ids after change
}
```
#### remove a group (.../{uuid}) - DELETE
Payload: None
Return: 
```
{
    success: boolean
}
```

##### control a group (.../{uuid}) - PUT
Payload: 
```
{
    state: { // to set group to 
        state: "on|off|toggle|blinking"
        if state is blinking:
            infinite: boolean,
            blinkCount: number,
            delay: number
    }
}
```
Return:
```
{
    state: { // group is now set to 
        state: "on|off|blinking"
        if state is blinking:
            infinite: boolean,
            blinkCount: number,
            delay: number
    }
}
```