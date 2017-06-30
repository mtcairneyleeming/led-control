import paho.mqtt.publish as mqttpub
import json
import config
config = config.conf()


def send(Id, state, infinite=None, delay=None, blink_count=None):
    if infinite is None or delay is None:
        # use simple as part of the advanced request is malformed
        send_simple(Id, state)
    else:
        send_advanced(Id, infinite, delay, blink_count)


def send_simple(Id, state):
    """Sends a simple state: "on"|"off"|"toggle", where state is a one of those values """
    data = json.dumps({'state': state})

    mqttpub.single('leds/' + Id + '/set_state_simple',
                   data, 1, False, config.mqtt_hostname, config.mqtt_port, None)


def send_advanced(Id, infinite, delay, blink_count):
    data = json.dumps({
        'state': 'blink',
        'infinite': "true" if infinite else "false",
        'delay': delay,
        'blinkCount': blink_count
    })
    mqttpub.single('leds/' + Id + '/set_state',
                   data, 1, False, config.mqtt_hostname, config.mqtt_port, None)
