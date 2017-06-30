#!/usr/bin/env python2
"""An MQTT server that receives state, wake and disconnect messages from devices and updates Redis."""
import json
from flask import Flask
from flask_redis import FlaskRedis
from flask_mqtt import Mqtt
from config import conf
from device_send import send_advanced, send_simple

app = Flask(__name__)
# use the free broker from HIVEMQ
app.config['MQTT_BROKER_URL'] = conf.mqtt_hostname
# default port for non-tls connection
app.config['MQTT_BROKER_PORT'] = conf.mqtt_port
# set the username here if you need authentication for the broker
app.config['MQTT_USERNAME'] = ''
# set the password here if the broker demands authentication
app.config['MQTT_PASSWORD'] = ''
# set the time interval for sending a ping to the broker to 5 seconds
app.config['MQTT_KEEPALIVE'] = 5
# set TLS to disabled for testing purposes
app.config['MQTT_TLS_ENABLED'] = False
# set redis url
app.config['REDIS_URL'] = "redis://@localhost:6379/0"

redis_store = FlaskRedis(app)
mqtt = Mqtt(app)


def getDeviceKey(Id):
    return 'device:{}'.format(Id)


def getGroupKey(Id):
    return 'group:{}'.format(Id)


def format_list(keys, states, led_descriptor="state"):
    output = []
    for i in range(len(states)):
        output.append({'id': keys[i], led_descriptor: json.loads(states[i])})
    return output


# MQTT handlers


@mqtt.on_message()
def handle_mqtt_message(client, userdata, message):
    parts = message.topic.split('/')
    if parts[-1] == "state":
        on_state_message(message)
    elif parts[-1] == "add":
        on_add_message(message)
    else:
        print("Error: message type not recognised: " + parts[-1])


def on_state_message(msg):
    # act upon state messages from devices
    data = json.loads(msg.payload)
    topic_parts = msg.topic.split("/")
    if hasattr(data, 'infinite'):
        stored_state = {'state': data['state'], 'blinkInfinite': data['infinite'],
                        'blinkCount': data['blinkCount'], 'blinkDelay': data['blinkDelay']}
        redis_store.set(getDeviceKey(data['id']), json.dumps(stored_state))

    else:
        redis_store.set(getDeviceKey(topic_parts[1]), json.dumps(
            {'state': data['state']}))
    print(redis_store.get(getDeviceKey(data['id'])))


def on_add_message(msg):
    # act upon state messages from devices
    print(msg.payload)
    data = json.loads(msg.payload)
    print("Device connected with id %s", data["uuid"])
    redis_store.set(getDeviceKey(data["uuid"]), json.dumps({'state': 'off'}))


# main

if __name__ == "__main__":
    mqtt.subscribe('leds/+/state')
    mqtt.subscribe('leds/manage/add')
    app.run(threaded=True)
