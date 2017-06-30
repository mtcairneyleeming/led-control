# pylint disable=C0111, C0103
"""A flask server that serves an API to control LEDs and serve data about them from Redis"""
import json
from flask import Flask, request
from flask.json import jsonify
from flask_redis import FlaskRedis

from device_send import send_advanced, send_simple

app = Flask(__name__)

# set redis url
app.config['REDIS_URL'] = "redis://@localhost:6379/0"

redis_store = FlaskRedis(app)


def get_device_key(Id):
    """Return the Redis key for a device based on its Id"""
    return 'device:{}'.format(Id)


def get_group_key(Id):
    """Return the Redis key for a group based on its uuid"""
    return 'group:{}'.format(Id)


def remove_prefix(key):
    """Strip the redis "device:" or "group:" prefix from a key prior to use in an API response"""
    if key[0] == "d":
        return key[7:]
    return key[6:]


def format_redis_output(keys, states, led_descriptor="state"):
    """Format Redis's output of a list of ids and a list of states
    into a structure suitable for the API """
    output = []
    for i in enumerate(states):
        output.append({
            'id': remove_prefix(
                keys[i]),
            led_descriptor: json.loads(states[i])})
    return output


@app.route('/')
def main():
    """Return an HTML main page (TODO)"""
    return ""


@app.route('/api/leds')
def list_leds():
    """Get all devices with their states.
    Warning: this uses KEYS, as it's simpler, but is also not recommended"""
    keys = redis_store.keys('device:*')
    leds = redis_store.mget(keys)
    full_list = format_redis_output(keys, leds)
    return jsonify(full_list)


@app.route('/api/leds', methods=['PUT'])
def set_many_leds():
    command_array = request.get_json(force=True)
    for command in command_array:
        if command['state']['state'] == "blink":
            data = command['state']
            stored_state = {'state': 'blinking',
                            'blinkInfinite': data['infinite'],
                            'blinkDelay': data['delay'],
                            'blinkCount': data['count']}
            redis_store.set(get_device_key(
                command['id']), json.dumps(stored_state))
            send_advanced(command['state'], data['infinite'],
                          data['delay'], data['count'])
        else:
            redis_store.set(get_device_key(command['id']),
                            json.dumps(command['state']))
            send_simple(command['id'], command['state']['state'])


@app.route('/api/leds/<Id>')
def get_led(Id):
    """Get one device with state based upon Id"""
    return redis_store.get(get_device_key(Id))


@app.route('/api/leds/<Id>', methods=['PUT'])
def set_state(Id):
    new_state = request.get_json(force=True)['state']
    stored_state = {'state': new_state}
    redis_store.set(get_device_key(Id), json.dumps(stored_state))
    send_simple(Id, new_state)
    return redis_store.get(get_device_key(Id))


@app.route('/api/leds/<Id>/blink', methods=['PUT'])
def set_blink(Id):
    data = request.get_json(force=True)
    stored_state = {'state': 'blinking',
                    'blinkInfinite': data['infinite'],
                    'blinkDelay': data['delay'],
                    'blinkCount': data['count']}
    redis_store.set(get_device_key(Id), json.dumps(stored_state))
    send_advanced(Id, data['infinite'], data['delay'], data['count'])
    return redis_store.get(get_device_key(Id))


@app.route('/api/groups')
def get_groups():
    """Get all groups with their LEDs.
    Warning: this uses KEYS, as it's simpler, but is also not recommended"""
    keys = redis_store.keys('group:*')
    leds = redis_store.mget(keys)
    full_list = format_redis_output(keys, leds, led_descriptor="data")
    return jsonify(full_list)


@app.route('/api/groups/<Id>')
def get_group(Id):
    return redis_store.get(get_group_key(Id))


@app.route('/api/groups/<Id>/leds')
def get_group_leds(Id):
    group = json.loads(get_group(Id))
    leds = []
    for led in group['leds']:
        leds.append(json.loads(redis_store.get(get_device_key(led))))
    return jsonify(leds)


@app.route('/api/groups/<Id>', methods=['PATCH'])
def edit_group(Id):
    """Warning - this operation is not atomic - there amy be issuses with duplicate leds
    if 2 edit requests are made is quick successsion"""
    group = json.loads(redis_store.get(get_group_key(Id)))
    changes = request.get_json(force=True)
    for add in changes['add']:
        if add not in group["leds"]:
            group["leds"].append(add)
    for remove in changes['remove']:
        if remove in group["leds"]:
            group["leds"].remove(remove)
    redis_store.set(get_group_key(Id), json.dumps(group))
    return group


@app.route('/api/groups', method=["POST"])
def create_group():
    data = request.get_json()
    with redis_store.pipeline() as pipe:
        while 1:
            try:
                # put a WATCH on the key that holds our sequence value
                pipe.watch("nextGroupId")
                # after WATCHing, the pipeline is put into immediate execution
                # mode until we tell it to start buffering commands again.
                # this allows us to get the current value of our sequence
                current_value = pipe.get('nextGroupId')
                pipe.incr("nextGroupId")

                next_value = int(current_value) + 1
                data["'id"] = next_value
                # now we can put the pipeline back into buffered mode with MULTI
                pipe.multi()
                pipe.set(get_group_key(next_value),
                         json.dumps(data))
                # and finally, execute the pipeline (the set command)
                pipe.execute()
                # if a WatchError wasn't raised during execution, everything
                # we just did happened atomically.
                break
            except FlaskRedis.WatchError:
                # another client will have created a group, so retry
                continue


# main
if __name__ == "__main__":
    app.run(threaded=True)
