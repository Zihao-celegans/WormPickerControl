
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
import time

# Anthony's import
from verifyDirlist import *
import cv2



"""         GENERAL CLASSES         """

# Define a custom dataset for the images that inherets from Dataset
# https://towardsdatascience.com/building-efficient-custom-datasets-in-pytorch-2563b946fd9f
# https://pytorch.org/tutorials/beginner/data_loading_tutorial.html

class ImageDataset(Dataset):

    # Override the 3 function init, len and getitem

    def __init__(self, data_root, classes, Nmax = 999999999, normtype = 0, input_layer_channels = 1):
        self.samples = []

        # Find the worm and background folders
        typefolders,typefolders_short = verifyDirlist(data_root,True)

        # Reverse the list so that worms loads first
        #typefolders.reverse()
        #typefolders_short.reverse()

        for (typefull,typeshort) in zip(typefolders,typefolders_short):
            
            # Load each image and append it and its type to the list
            # don't use verifyDirlist for this huge folder, takes too long to filter by file type

            imagefiles = os.listdir(typefull)

            #Nmax = min((len(imagefiles),Nmax))-1
                

            # Regardless (worm or background images) shuffle the names before loading in case we're only going to be loading a subset
            # Matching the size of worms, and we want it representative of the broad dataset

            print("Shuffling ", typeshort, " images before loading subset...")
            random.shuffle(imagefiles)

                
            
            for (i,imgname) in enumerate(imagefiles):

                # Set read type
                if input_layer_channels == 3:
                    read_type = cv2.IMREAD_COLOR

                elif input_layer_channels == 1:
                    read_type = cv2.IMREAD_GRAYSCALE

                else:
                    print('Invalid input layer channel count')
                    exit(-1)

                # Load image, save transpose for later
                img = cv2.imread(os.path.join(typefull,imgname),read_type)
                #img = np.reshape(img,(input_layer_channels,img.shape[0],img.shape[1]))


                # Make sure the image is valid, if not e.g. constant) add a tiny amount of random noise instead of skipping
                MAX = np.max(img)

                if MAX == 0 or MAX == np.min(img) or np.isnan(MAX):
                    img[0,0] = MAX-1
                    

                # Normalize the image and convert to float
                img = img.astype(np.float)

                if normtype == 1:

                    # normalize21
                    img = img - np.min(img)
                    img = img / np.max(img)

                elif normtype == 2:

                    # Subtract the mean
                    img = img - np.mean(img)


                # Transpose normalized image as tensor
                img = torch.from_numpy(img).permute(2,1,0)

                # Set the activations for this type (which class it is)
                for c in range(len(classes)):
                    if typeshort == classes[c]:
                        activations = c

                # Add it to the list
                self.samples.append((img,typeshort,activations))

                # Update user 
                if i%1000 == 0:
                    print('Loaded ', typeshort , ' # ', i, ' of ', len(imagefiles), " - Nmax=", Nmax)

                # Quit if Nmax reached (so that N background == N worms)
                if i>= Nmax:
                    break

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        return self.samples[idx]



# Define a neural network, for 3-channel images

class Net(nn.Module):

    def __init__(self,img_size,ker_size, ln = [1, 6, 16, 32, 200, 84, 2], Nconvlayers = 3):

        self.img_size = img_size
        self.ksloss =  ker_size - 1
        self.Nconvlayers = Nconvlayers
        self.ln = ln # layer numbers

        self.conv3outsize = self.img_size - self.ksloss * self.Nconvlayers # 2 pixels lost at each layer (FOR 3x3 KERNAL ONLY, for 5x5 would be 4 lost), 3 conv layers

        super(Net, self).__init__()
        self.conv1 = nn.Conv2d(ln[0], ln[1], ker_size) # 1 = input channels, 6 = output channels, 3x3 = conv kernel size -> out image size is N-2 (e.g. 20->18)
        self.pool = nn.MaxPool2d(1, 1)
        self.conv2 = nn.Conv2d(ln[1], ln[2], ker_size) # output is 16 channels for 6x6 images, so 16 * 6 * 6 pixels -> out image size is N-2-2 (e.g. 20->18->16)
        self.conv3 = nn.Conv2d(ln[2], ln[3], ker_size) # -> out image size is N-2-2-2 (e.g. 20->18->16->14)
        #self.conv4 = nn.Conv2d(32,64, ker_size)
        self.fc1 = nn.Linear(ln[3] *  self.conv3outsize *  self.conv3outsize, ln[4]) 
        self.fc2 = nn.Linear(ln[4],ln[5])
        self.fc3 = nn.Linear(ln[5], ln[6])

    def forward(self, x):

        # Enforce correct image size
        if (x.shape[2] != self.img_size ) or (x.shape[3] != self.img_size):
            print('\n\n\nERROR: You loaded an image of size: ' , x.shape[2], "\nBut the network was initialized with image size: ", self.img_size, "\n\n\n")
            

        x0 = self.conv1(x)
        x1 = F.relu(x0)
        x = self.pool(x1)
        # x = self.pool(F.relu(self.conv1(x)))
        x = self.pool(F.relu(self.conv2(x)))
        x = self.pool(F.relu(self.conv3(x)))
        #x = self.pool(F.relu(self.conv4(x)))
        x = x.view(-1, self.ln[3] *  self.conv3outsize *  self.conv3outsize)              # Transform the image into a layer, this is the output size of each image after 2 rounds conv  
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        x = self.fc3(x)
        return x

    def num_flat_features(self, x):
        size = x.size()[1:]  # all dimensions except the batch dimension
        num_features = 1
        for s in size:
            num_features *= s
        return num_features




"""         Helper functions         """

def testImshow(img):
    #img = img / 2 + 0.5     # unnormalize
    npimg = img.numpy()
    plt.imshow(np.transpose(npimg, (1, 2, 0)))
    plt.show()


def showSampleImages(trainloader):

    # get some random training images
    dataiter = iter(trainloader)
    images, labels, activations = dataiter.next()

    # show images and print labels
    print(labels)
    testImshow(torchvision.utils.make_grid(images))

# Calculate the accuracy of the CNN
def testCNN(testloader,net,device,criterion,start_time = time.time()):
    correct = 0
    total = 0
    loss = 0
    iters = 0
    with torch.no_grad():
        for data in testloader:
            images, labels, activations = data[0].to(device), data[1], data[2].to(device)  # GPU version
            # np_images = images.numpy()
            #print(images)
            #cv2.imshow("images.numpy()[0]", images.numpy()[0][0])
            #cv2.waitKey(0)
            #cv2.imshow("images.numpy()[1]", images.numpy()[1][0])
            #cv2.waitKey(0)
            
            outputs = net(images.float())
            _, predicted = torch.max(outputs.data, 1)
            total += activations.size(0)
            correct += (predicted == activations).sum().item()

            # Calculate validation loss
            loss += criterion(outputs, activations)
            iters += 1
    
    accuracy = 100.0 * float(correct) / float(total)
    print('\n----------> Test loss: %0.4f - Test accuracy: %0.2f %% - Epoch time: %0.2f min\n' % 
          (float(loss)/float(iters),accuracy,(time.time()-start_time)/60.0))
    return accuracy


    # Load a saved CNN
def loadCNN(fname,img_size = 20,ker_size = 3, net_structure = [ 3, 6, 16, 32, 200, 84, 2 ]):
    #[1, 6, 16, 32, 200, 84, 2]

    # Load any checkpoint
    if torch.cuda.is_available():
        map_location=lambda storage, loc: storage.cuda()
    else:
        map_location='cpu'
    checkpoint = torch.load(fname, map_location=map_location)

    # Initialize a network and then load satate dict from the checkpoint
    net = Net(img_size, ker_size, net_structure)
    net.load_state_dict(checkpoint['model_state_dict'])

    return net

