import paho.mqtt.client as mqtt

TOPIC = "FAN_221011/temperature"

mqttc = mqtt.Client("client2")
mqttc.connect("test.mosquitto.org", 1883)
mqttc.publish(TOPIC, "abcdefg!!!!!!!!")