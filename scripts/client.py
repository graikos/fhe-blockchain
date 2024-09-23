import message_pb2
import sys
import socket
import json

msg = message_pb2.HelloMsg()
msg.greet = "Hello gio"
msg.id = 134

config_path = "../config/config.json"
with open(config_path) as f:
    config = json.load(f)


print(msg.SerializeToString())

if (len(sys.argv) == 3):
    config["net"]["address"] = sys.argv[1]
    config["net"]["port"] = int(sys.argv[2])



client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
print("127.0.0.1", config["net"]["port"])
client.connect(("127.0.0.1", config["net"]["port"]))

serialized_msg = msg.SerializeToString()
s = len(serialized_msg).to_bytes(4, 'big')
t = '\x00\x00\x00\x00'

print(s)
print(t.encode())
print(client)

client.sendall(s)
client.sendall(t.encode())
client.sendall(msg.SerializeToString())

input()
data = client.recv(1024)
print(data)