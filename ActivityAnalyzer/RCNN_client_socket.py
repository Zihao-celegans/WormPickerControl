"""
    Client - triggers RCNN_Segment_Image which is a server

    Replaced by using Boost socket instead of this script when triggering python 
    
    Adapted from:
    https://stackoverflow.com/questions/23148278/send-commands-from-command-line-to-a-running-python-script-in-unix

"""
import socket
import struct
import sys
import numpy as np
from RCNN_Segment_Image import encodeImageForSocket, decodeImageFromSocket, \
    decodeGender, MAX_LENGTH
import cv2

print('starting socket')
HOST = '127.0.0.1'
PORT = 10000
s = socket.socket()
s.connect((HOST, PORT))

# img_path = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderDatasets\ADF-05 Dataset for MaskRCNN\ImagesToPredict\PDB-01-ImagesToPredict\2021-06-07 (18-08-09)_0.png' #2 Herms with head and tails
#img_path = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderDatasets\ADF-05 Dataset for MaskRCNN\ImagesToPredict\PDB-01-ImagesToPredict\2021-06-07 (18-06-25)_0.png' #1 blurry worm, worm net can't identify it
#img_path = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderDatasets\ADF-05 Dataset for MaskRCNN\ImagesToPredict\PDB-01-ImagesToPredict\2021-06-07 (18-28-46)_0.png' # Overlapping worms
#img_path = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderDatasets\June2021ADF_dataset_plus_PDB03Partial\Evaluation\herm_009_20.png'
img_path = r'C:\Users\yuyin\Desktop\Penn\Research\Fang-Yen-Lab\Autogans\Data\GenderTestDatasetForEvalution\DatasetImages\2021-06-24 (08-55-59)_0.png'

test_img = cv2.imread(img_path, cv2.IMREAD_GRAYSCALE)
encoded_img = encodeImageForSocket(test_img)
print(f'img encoded')

while 1:
    # # s.send(packed_data)
    s.send(encoded_img.encode())

    result = s.recv(MAX_LENGTH)

    gender_info = decodeGender(result)
    print(gender_info)
    image_result = decodeImageFromSocket(result)
    cv2.imshow("Returned Image", image_result)
    cv2.waitKey(0)
