import requests

"""

init    level 0 , vertical 0
left    level + 30
right   level - 30
top     vertical + 30
bottom  vertical - 30

"""

cmd = "init"

r = requests.post("http://192.168.107.176/command", data="init")


