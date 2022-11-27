""" 
    test PyTorch
    Must use Anaconda 2019.10 environment 
        See OneNote for configuration
        https://pytorch.org/tutorials/beginner/blitz/tensor_tutorial.html#sphx-glr-beginner-blitz-tensor-tutorial-py


    --------------------------------------------------------------------------------


    Parameters used for worm-background as of 2/24/2020:
    Img size: 20
    Ker size: 3
    Norm type: 1
    Optimizer: ADAM
    
    --------------------------------------------------------------------------------

    Parameters used for Contaminated-Clean as of 4/1/2020:

        # Parameters - load or create network
        pnet = r'I:\CheckPoint'
        load_net = os.path.join(pnet,r'C:\Dropbox\WormWatcher (shared data)\Well score training\checkpoints_contamination\net_Transfer3_WW1-03_88.73.pth')
        use_loaded_net = True

        # Parameters - training and testing data locations
        ptrain = r'F:\All available data for testing'
        ptest = r'F:\All available data for testing'
        pvalid = ptest
        classes = ('Contaminated', 'Clean') # Must match folder names

        # Parameters - training details

        img_size = 85
        ker_size = 5
        load_limit_train = 1175 # Set maximum image set size to load for each type
        load_limit_test = 1175
        batch_size = 24;
        num_epochs = 500;
        use_cuda = 0
        learn_rate = 0.002
        _momentum = 0.9 # SGD only

        normtype = 1 # 0 = none, 1= normalize21, 2= subtract mean


    ---------------------------------------------------------------------------------

     # Parameters - training and testing data locations
    ptrain = r'C:\WMtrain'
    ptest = r'C:\WMtest'
    pvalid = ptest
    classes = ('well', 'background') # Must match folder names

    # Parameters - training details

    img_size = 20
    ker_size = 3
    load_limit_train = 50000 # Set maximum image set size to load for each type\
    load_limit_test = 50000
    batch_size = 24;
    num_epochs = 500;
    use_cuda = 0
    learn_rate = 0.001
    _momentum = 0.9 # SGD only

    normtype = 1 # 0 = none, 1= normalize21, 2= subtract mean


    Parameters used for Wormotel-Background as of 2/24/2020:
    



"""

# Torch
import torch
import torchvision
import torchvision.transforms as transforms

# Torch sub-modules
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torch.utils.data import Dataset

# Other
import matplotlib.pyplot as plt
import numpy as np
import os
import random
import cv2
import time
import json

# Anthony's import
from verifyDirlist import *
from CNN_Helper import *
from CNN_exportToONNX import exportONNX


if __name__ == '__main__':

    # Parameters - the ONLY value you need to edit is the pdata
    pdata = r'C:\CNN_Datasets\Head_Tail_Gender_CNN_08'

    # Find train_params.json which must be in pdata
    with open(os.path.join(pdata,'train_params.json'),"r") as j:
        tree = json.load(j)

    # Paths related to pdata
    pnet = os.path.join(pdata,'checkpoint')
    ptrain = os.path.join(pdata,'train')
    ptest = os.path.join(pdata,'test')
    pvalid = ptest

    if not os.path.exists(pnet):
        os.mkdir(pnet)

    # Load from JSON - continue existing network info
    fnet = tree["loaded_net"]
    load_net = os.path.join(pnet,fnet)
    use_loaded_net = tree["use_loaded"]

    # Load from JSON - network and dataset parameters
    classes = tree["classes"]
    img_size = tree["img_size"]
    ker_size = tree["ker_size"]
    load_limit_train = tree["load_limit_train"]
    load_limit_test = tree["load_limit_test"]
    batch_size = tree["batch_size"]
    use_cuda = tree["use_cuda"]
    learn_rate = tree["learn_rate"]
    _momentum = tree["momentum"] # SGD only
    normtype = tree["norm_type"] # 0 = none, 1= normalize21, 2= subtract mean
    net_structure = tree["net_structure"]
    Nconvlayers = tree["Nconvlayers"]
    num_epochs = 35; # No need to change
    num_workers = 0 # Set to 0 to debug broken pipe errors or 2 otherwise



    """ STEP 0: VERIFY CUDA DEVICE """

    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    if use_cuda and torch.cuda.is_available() :
        device = torch.device('cuda:0')
        torch.set_default_tensor_type('torch.cuda.FloatTensor') # IF using cuda create all new tensors on cuda
        # Seems to avoid an error when load state dict loads to CPU for some reason




    elif use_cuda:
        print('CUDA requested but CUDA Device not detected. Training Aborted...')
        exit(0)
    else:
        device = torch.device('cpu')

    print(device)

    """ STEP 1: LOAD DATA """

    # Define training and testing sets and classes
    trainset = ImageDataset(ptrain,classes,load_limit_train,normtype,net_structure[0])
    trainloader = torch.utils.data.DataLoader(trainset, batch_size=batch_size, shuffle=True, num_workers=num_workers)

    testset = ImageDataset(ptest,classes,load_limit_test,normtype,net_structure[0])
    testloader = torch.utils.data.DataLoader(testset, batch_size=batch_size, shuffle=True, num_workers=num_workers)

    validset = ImageDataset(pvalid,classes,min((load_limit_test,load_limit_test)),normtype,net_structure[0])
    validloader = torch.utils.data.DataLoader(validset, batch_size=batch_size, shuffle=True, num_workers=num_workers)

    


    # Show some images for fun
    # showSampleImages(trainloader)

    """ STEP 2: CREATE OR LOAD CNN, LOSS FCN, OPTIMIZER """


    # Create basic or load
    net = Net(img_size,ker_size,net_structure, Nconvlayers)

    if use_loaded_net:
        
        # Load the overall checkpoint
        checkpoint = torch.load(load_net)

        # Load the network state
        net.load_state_dict(checkpoint['model_state_dict'])

        # Set a basic optimizer
        optimizer = optim.SGD(net.parameters(), lr=learn_rate, momentum=_momentum)
        #optimizer = optim.Adam(net.parameters(),lr=learn_rate)
        #optimizer = optim.Adagrad(net.parameters(),lr=learn_rate)
        #optimizer = optim.ASGD(net.parameters(),lr=learn_rate)

        # Load the optimizer state
        optimizer.load_state_dict(checkpoint['optimizer_state_dict'])

        # Load the starting epoch
        start_epoch = checkpoint['epoch']
        loss = checkpoint['loss']

    else:
        # Use the basic optimizer and set start epoch to 0
        optimizer = optim.SGD(net.parameters(), lr=learn_rate, momentum=_momentum)
        #optimizer = optim.Adam(net.parameters(),lr=learn_rate)
        #optimizer = optim.Adagrad(net.parameters(),lr=learn_rate)
        #optimizer = optim.ASGD(net.parameters(),lr=learn_rate)


        start_epoch = 0

    # Set criterion
    criterion = nn.CrossEntropyLoss()

    # Transfer to CUDA device
    net = net.float()
    net.to(device)

    # Pre-test
    testCNN(validloader,net,device,criterion)

    """ STEP 3: TRAIN THE NETWORK """


    for epoch in range(start_epoch,num_epochs):  # loop over the dataset multiple times
        start_time = time.time()
        running_loss = 0.0
        running_total = 0;
        running_correct = 0;

        for i, data in enumerate(trainloader, 0):
            # get the inputs; data is a list of [inputs, labels], but transfer them to GPU
            # inputs, labels, activations = data # CPU version
            inputs, labels, activations = data[0].to(device), data[1], data[2].to(device)  # GPU version
            #fout = os.path.join('E:\CNN_Datasets\Worm_Motion_test\image',str(i)+'.png')
            #torchvision.utils.save_image(inputs, fout)

            # zero the parameter gradients
            optimizer.zero_grad()

            # forward + backward + optimize
            outputs = net(inputs.float())
            loss = criterion(outputs, activations)
            loss.backward()
            optimizer.step()

            # print loss statistics every 25% of an epoch
            running_loss += loss.item()
            _, predicted = torch.max(outputs.data, 1)
            running_total += predicted.size(0)
            running_correct += (predicted == activations).sum().item()
            iternum = int(len(trainset)/batch_size*0.25)
            if iternum != 0 and i % iternum == iternum-1:   

                # Loss on training data
                print('[%d, %5d] loss: %.4f - acc: %0.2f' %(epoch + 1, i + 1, running_loss / iternum, running_correct/running_total))
                running_loss = 0.0
                running_total = 0;
                running_correct = 0;

        # Print the validation accuracy every epoch
        accuracy = testCNN(validloader,net,device,criterion,start_time)

        # Save network every epoch
        outpath =os.path.join(pnet,'net_' + str(epoch)  + "_" + str(round(accuracy,3)) + '.pth')
        torch.save({
            'epoch': epoch,
            'model_state_dict': net.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'loss': loss},
            outpath)

        # Convert to ONNX every epoch
        exportONNX(outpath,img_size,ker_size,net_structure)



    print('Finished Training')



    """ STEP 4: TEST ON TEST DATA """
    testCNN(testloader,net,device,criterion)


