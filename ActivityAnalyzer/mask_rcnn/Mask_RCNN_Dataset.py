import os
import numpy as np
import torch
import cv2
from PIL import Image

import sys
# Add the utils folder to the path so we can import the helper modules
# Note that this is done relative to the mask_rcnn folder so it should work regardless of your repo directory
utils_path = sys.path[0].split("\\")[:-1] # Get the current path to the script and remove the mask_rcnn directory
base_path = '\\'.join(utils_path) # Create the path to the base folder
utils_path.append('mask_rcnn_utils') # append the utils folder to the path
utils_path = '\\'.join(utils_path) # create the final path string to the utils folder
sys.path.append(base_path) # Add the base folder to the system's path
sys.path.append(utils_path) # Add the utils folder to the system's path

import utils as U





class WormDataset(torch.utils.data.Dataset):
    '''
    Defines a dataset to be used with the Mask RCNN network
    '''


    # Constructor, 
    def __init__(self, root, transforms, num_classes, class_parameters=None):
        '''
        root: root folder of the dataset containing the DatasetImages and DatasetMasks folders.
        transforms: any transforms to be performed on the dataset images. We have been doing our own augmentations so this typically only converts the images to tensors.
        num_classes: the number of classes for the datset. Right now this only matters if its 1 because if it is then we flatten the output labels to all be the same.
        class_parameters: the encoding parameters for the masks. Should be in the format: {ClassId: (label, start_val, end_val, channel)}. If no params are provided then the dataset will default to the parameters from util.py "get_labeling_params" method.
        '''
        self.root = root
        self.transforms = transforms
        # find all image files, sorting them to
        # ensure that they are aligned
        self.imgs = list(sorted(os.listdir(os.path.join(root, "DatasetImages"))))
        self.masks = list(sorted(os.listdir(os.path.join(root, "DatasetMasks"))))
        self.num_classes = num_classes
        if class_parameters is None:
            self.class_params = U.get_labeling_params()
        else:
            self.class_params = class_parameters

        print(f'label params: {self.class_params}')


    def __getitem__(self, idx):
        '''
        This method is implicitly called by PyTorch during training. It will take an image and its corresponding mask from the dataset, decode the mask according to the labeling parameters of the dataset, and return the image along with its targets.
        '''

        # load images and masks
        img_path = os.path.join(self.root, "DatasetImages", self.imgs[idx])
        mask_path = os.path.join(self.root, "DatasetMasks", self.masks[idx])
        img = Image.open(img_path).convert("RGB")
        # See constructor comments for encoding of masks.
        mask = Image.open(mask_path)
        #mask = Image.open(r'C:\Dropbox\FramesData\FramesForAnnotation\train\Renamed\021 - Copy_mask.png')
        # convert the PIL Image into a numpy array
        mask = np.array(mask) # RGB shape (512, 612, 3)
        #print(f'\t\t$$$$$$$$$$$$$$$$$ Shape: {mask.shape}')

        #exclude.append('Egg') # For now just hard code to exclude eggs because the PyTorch training is calling getitem so I can't add the exclude filter to the call.

        #class_params = {0 : ["L1-Larva",1,51,0], 1: ["L2-Larva",52,102,0], 2 : ["L3-Larva",103,153,0], 3 : ["L4-Herm",154,204,0], 4 : ["L4-Male",205,255,0], 5 : ["Adult-Herm",1,51,1], 6 : ["Adult-Male",52,102,1], 7: ["Unkown-Stage", 205, 255, 2]} # Class ID : (Class Name, Start Value, Max Value, Channel)
        #class_params = U.get_labeling_params()
        #class_params = {x : class_params[x] for x in class_params if class_params[x][0] not in exclude} # Filter out the unwanted labels
        #print(f'params after exclude: {class_params}')

        mask_labels = []
        label_names = []
        channel_masks = []
        # The masks are contained within each channel of the image, so loop through each channel
        # get the labels for each mask, and then generate a binary mask for each instance in the channel.
        for channel in range(3):
            this_channel = mask[:,:,channel]
            this_obj_ids = np.unique(this_channel) # Get the instance ids for this channel
            this_obj_ids = this_obj_ids[1:] # First id is the background, so remove it

            # Assign the labels for each instance according to this channel
            this_channel_labels = [x for x in self.class_params if self.class_params[x][3] == channel]
            #print(f'Channel: {channel}, labels: {this_channel_labels}')
            ids_to_remove = []
            for id in this_obj_ids:
                for index, lbl in enumerate(this_channel_labels):
                    if id >= self.class_params[lbl][1] and id <= self.class_params[lbl][2]:
                        mask_labels.append(lbl)
                        label_names.append(self.class_params[lbl][0])
                        break
                    if index == len(this_channel_labels) - 1:
                        #print(f'Excluded label found. Id: {id}, channel: {channel}')
                        ids_to_remove.append(id) # If we reach here then the id was not found in the class parameters (which means its been excluded)

            # Remove the excluded labels:
            this_obj_ids = np.array([x for x in this_obj_ids if x not in ids_to_remove])

            this_channel_masks = this_channel == this_obj_ids[:, None, None]
            channel_masks.append(this_channel_masks)

            #print(f'mask_labels: {mask_labels}')
            #print(f'labels names: {label_names}')

        # Now that we have all the masks, concatenate them into one numpy array
        masks = channel_masks[0]
        for m in range(1,len(channel_masks)):
            masks = np.concatenate((masks, channel_masks[m]))

        #print(f'Final masks shape after concat: {masks.shape}')

        ##print(f"&&&&&&&&&&&& Shape: {mask.shape}")
        ## instances are encoded as different colors
        #obj_ids = np.unique(mask)
        ##print(f"****************************** image: {self.imgs[idx]}")
        ##print(f"****************************** mask: {self.masks[idx]}")
        ##print(f"****************************************************************Unique IDs for image {idx}: {obj_ids}")

        ## first id is the background, so remove it
        #obj_ids = obj_ids[1:]

        ## split the color-encoded mask into a set
        ## of binary masks
        #masks = mask == obj_ids[:, None, None]
        #print(f'\t\t$$$$$$$$$$$$$$$$$ Shape: {masks.shape}')
        #print(f'\t\t$$$$$$$$$$$$$$$$$ masks: {masks}')

        # get bounding box coordinates for each mask
        #num_objs = len(obj_ids)
        num_objs = masks.shape[0]
        boxes = []
        for i in range(num_objs):
            pos = np.where(masks[i])
            xmin = np.min(pos[1])
            xmax = np.max(pos[1])
            ymin = np.min(pos[0])
            ymax = np.max(pos[0])
            boxes.append([xmin, ymin, xmax, ymax])

        # convert everything into a torch.Tensor
        boxes = torch.as_tensor(boxes, dtype=torch.float32)

        if self.num_classes == 1:
            labels = torch.ones((num_objs,), dtype=torch.int64) # For use with models with only 1 class
        else:
            labels = torch.as_tensor(mask_labels, dtype=torch.int64) # For use with multi-class models

        #print(f'image {idx} path: {img_path}')
        #print(f'image {idx} labels: {labels}')
        #print(f'\t\t$$$$$$$$$$$$$$$$$ labels: {labels.shape}')
        masks = torch.as_tensor(masks, dtype=torch.uint8)

        image_id = torch.tensor([idx])
        area = (boxes[:, 3] - boxes[:, 1]) * (boxes[:, 2] - boxes[:, 0])
        # suppose all instances are not crowd
        iscrowd = torch.zeros((num_objs,), dtype=torch.int64)

        target = {}
        target["boxes"] = boxes
        target["labels"] = labels
        target["masks"] = masks
        target["image_id"] = image_id
        target["area"] = area
        target["iscrowd"] = iscrowd


        if self.transforms is not None:
            img, target = self.transforms(img, target)

        return img, target


    def __len__(self):
        return len(self.imgs)

    #@staticmethod
    #def get_labeling_params(exclude=[]):
    #    class_params = {1 : ["L1-Larva",1,51,0], 2: ["L2-Larva",52,102,0], 3 : ["L3-Larva",103,153,0], 4 : ["L4-Herm",154,204,0], 5 : ["L4-Male",205,255,0], 6 : ["Adult-Herm",1,51,1], 7 : ["Adult-Male",52,102,1], 8 : ["Dauer",103,153,1] , 9: ["Egg", 154, 204, 1], 10: ["Unknown-Stage", 205, 255, 2]} # Class ID : (Class Name, Start Value, Max Value, Channel)

    #    class_params = {x : class_params[x] for x in class_params if class_params[x][0] not in exclude} # Filter out the unwanted labels
    #    return class_params
