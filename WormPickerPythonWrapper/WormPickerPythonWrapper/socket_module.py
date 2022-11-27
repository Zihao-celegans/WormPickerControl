'''
    socket_module.py

    Utilities for socket communication with WormPicker

    Zihao Li & Siming He
    Fang-Yen Lab
    Dec 2021
'''

import enum
import socket

class SocketType (enum.Enum):
    SERVER = 1
    CLIENT = -1

class socket_module: 

    def __init__(self, ip, port, socket_type, size=4096, format="utf-8"):
        self.ip = ip
        self.port = port
        self.size = size
        self.format = format
        self.type = socket_type
        self.conn = 0
        self.addr = 0

        if (self.type == SocketType.SERVER):
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.bind((self.ip, self.port))
            self.socket.listen(10)
            (self.conn, self.addr) =  self.socket.accept()

        elif(self.type == SocketType.CLIENT):
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.ip, self.port))


    def receive_data(self, show_message = False):
        if (self.type == SocketType.CLIENT):
            data = self.socket.recv(self.size).decode(self.format)
            if (show_message == True): print("[SERVER SENT] " + str(data))
            return data
        elif(self.type == SocketType.SERVER):
            data = self.conn.recv(self.size).decode(self.format)
            if (show_message == True): print("[CLIENT SENT] " + str(data))
            return data

    def send_data(self, data, delimiter = "\n"):

        if (self.type == SocketType.CLIENT):
            self.socket.send((data + delimiter).encode(self.format))

        elif(self.type == SocketType.SERVER):
            self.conn.send((data + delimiter).encode(self.format))

    def close_socket(self):
        self.socket.close()

        