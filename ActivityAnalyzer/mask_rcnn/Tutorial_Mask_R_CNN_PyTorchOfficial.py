"""
    Adapted from: 
    https://pytorch.org/tutorials/intermediate/torchvision_tutorial.html

    Fine-tune a pre-trained mask R-CNN model for detecting pedestrians. Dataset has
    170 images with 345 instances of pedestrians


    For Windows, please install pycocotools from gautamchitnis with command
    pip install git+https://github.com/gautamchitnis/cocoapi.git@cocodataset-master#subdirectory=PythonAPI

"""


"""
    Dataset definition - inherits from pytorch dataset just like Dataset in CNN_Helper:
    __getitem__ must return:
       - Image: a PIL image of size (H,W)
       - Target: A dict with the following fields

            * boxes(FloatTensor[N,4]), coordinates of N bboxes [x0,y0,x1,y1], ranged in bounds only
            * labels(Int64Tensor[N]), Label for each bbox, 0 is background
                                       Background is default, labels tensor should only have non-bg classes
                                       IF your data doesn't have a background. 

            * image_id(Int64Tensor[1]), Image identified, unique between all images in dataset
            * area (Tensor[N]), area of each of the N bboxes. Used for evaluation
            * iscrowd(UInt8Tensor[N]), any True will be ignored during evaluation (?)
            * OPTIONAL: masks(Uint8Tensor[N,H,W]), segmentation masks for each object
            * OPTIONAL: keypoints(FloatTensor[N,K,3]), For each of N objects, have K keypoints in format
                            [x,y,visibility], defining object. visibility = 0 means not visible. for data
                            augmentation, flipping a keypoint depends on data representation, adapt 
                            transforms.py for your new keypoint representation

    As long as model returns above, it will work for training and evaluation (using pycocotools).

"""



import os
import numpy as np
import torch
from PIL import Image, ImageDraw
import cv2
import random
from datetime import datetime
from Mask_RCNN_Dataset import *
from Mask_RCNN_Train import *

import sys
# Add the utils folder to the path so we can import the helper modules
# Note that this is done relative to the mask_rcnn folder so it should work regardless of your repo directory
utils_path = sys.path[0].split("\\")[:-1] # Get the current path to the script and remove the mask_rcnn directory
base_path = '\\'.join(utils_path) # Create the path to the base folder
utils_path.append('mask_rcnn_utils') # append the utils folder to the path
utils_path = '\\'.join(utils_path) # create the final path string to the utils folder
sys.path.append(base_path) # Add the base folder to the system's path
sys.path.append(utils_path) # Add the utils folder to the system's path

import transforms as T
import utils as U
import engine as E


def random_colour_masks(image):
    #print(f'shape: {image.shape}')
    colours = [[0, 255, 0],[0, 0, 255],[255, 0, 0],[0, 255, 255],[255, 255, 0],[255, 0, 255],[80, 70, 180],[250, 80, 190],[245, 145, 50],[70, 150, 250],[50, 190, 190]]
    #r = np.zeros_like(image).astype(np.uint8)
    #g = np.zeros_like(image).astype(np.uint8)
    #b = np.zeros_like(image).astype(np.uint8)

    r,g,b = image[:,:,0], image[:,:,1], image[:,:,2]

    color = colours[random.randrange(len(colours))]

    r[r > 0], g[g > 0], b[b > 0] = color
    #coloured_mask = np.stack((r, g, b), axis=2)
    #print(f'Color mask: {coloured_mask}')
    #color_mask = np.array(coloured_mask)
    #cv2.imshow('Colored mask', image)
    #cv2.waitKey(0)
    #test_image_mask = Image.fromarray(coloured_mask)
    #test_image_mask.show()
    #return coloured_mask
    return image, color

#class WormDataset(torch.utils.data.Dataset):


#    # Constructor, 
#    def __init__(self, root, transforms, num_classes, class_parameters=None):
#        self.root = root
#        self.transforms = transforms
#        # find all image files, sorting them to
#        # ensure that they are aligned
#        self.imgs = list(sorted(os.listdir(os.path.join(root, "DatasetImages"))))
#        self.masks = list(sorted(os.listdir(os.path.join(root, "DatasetMasks"))))
#        self.num_classes = num_classes
#        if class_parameters is None:
#            self.class_params = U.get_labeling_params()
#        else:
#            self.class_params = class_parameters


#    # Getitem, see notes above
#    def __getitem__(self, idx):

#        # load images and masks
#        img_path = os.path.join(self.root, "DatasetImages", self.imgs[idx])
#        mask_path = os.path.join(self.root, "DatasetMasks", self.masks[idx])
#        img = Image.open(img_path).convert("RGB")
#        # note that we haven't converted the mask to RGB,
#        # because each pixel value corresponds to a different instance
#        # with 0 being background
#        mask = Image.open(mask_path)
#        #mask = Image.open(r'C:\Dropbox\FramesData\FramesForAnnotation\train\Renamed\021 - Copy_mask.png')
#        # convert the PIL Image into a numpy array
#        mask = np.array(mask) # RGB shape (512, 612, 3)
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ Shape: {mask.shape}')

#        #exclude.append('Egg') # For now just hard code to exclude eggs because the PyTorch training is calling getitem so I can't add the exclude filter to the call.

#        #class_params = {0 : ["L1-Larva",1,51,0], 1: ["L2-Larva",52,102,0], 2 : ["L3-Larva",103,153,0], 3 : ["L4-Herm",154,204,0], 4 : ["L4-Male",205,255,0], 5 : ["Adult-Herm",1,51,1], 6 : ["Adult-Male",52,102,1], 7: ["Unkown-Stage", 205, 255, 2]} # Class ID : (Class Name, Start Value, Max Value, Channel)
#        #class_params = U.get_labeling_params()
#        #class_params = {x : class_params[x] for x in class_params if class_params[x][0] not in exclude} # Filter out the unwanted labels
#        #print(f'params after exclude: {class_params}')

#        mask_labels = []
#        label_names = []
#        channel_masks = []
#        # The masks are contained within each channel of the image, so loop through each channel
#        # get the labels for each mask, and then generate a binary mask for each instance in the channel.
#        for channel in range(3):
#            this_channel = mask[:,:,channel]
#            this_obj_ids = np.unique(this_channel) # Get the instance ids for this channel
#            this_obj_ids = this_obj_ids[1:] # First id is the background, so remove it

#            # Assign the labels for each instance according to this channel
#            this_channel_labels = [x for x in self.class_params if self.class_params[x][3] == channel]
#            #print(f'Channel: {channel}, labels: {this_channel_labels}')
#            ids_to_remove = []
#            for id in this_obj_ids:
#                for index, lbl in enumerate(this_channel_labels):
#                    if id >= self.class_params[lbl][1] and id <= self.class_params[lbl][2]:
#                        mask_labels.append(lbl)
#                        label_names.append(self.class_params[lbl][0])
#                        break
#                    if index == len(this_channel_labels) - 1:
#                        #print(f'Excluded label found. Id: {id}, channel: {channel}')
#                        ids_to_remove.append(id) # If we reach here then the id was not found in the class parameters (which means its been excluded)

#            # Remove the excluded labels:
#            this_obj_ids = np.array([x for x in this_obj_ids if x not in ids_to_remove])

#            this_channel_masks = this_channel == this_obj_ids[:, None, None]
#            channel_masks.append(this_channel_masks)

#            #print(f'mask_labels: {mask_labels}')
#            #print(f'labels names: {label_names}')

#        # Now that we have all the masks, concatenate them into one numpy array
#        masks = channel_masks[0]
#        for m in range(1,len(channel_masks)):
#            masks = np.concatenate((masks, channel_masks[m]))

#        #print(f'Final masks shape after concat: {masks.shape}')

#        ##print(f"&&&&&&&&&&&& Shape: {mask.shape}")
#        ## instances are encoded as different colors
#        #obj_ids = np.unique(mask)
#        ##print(f"****************************** image: {self.imgs[idx]}")
#        ##print(f"****************************** mask: {self.masks[idx]}")
#        ##print(f"****************************************************************Unique IDs for image {idx}: {obj_ids}")

#        ## first id is the background, so remove it
#        #obj_ids = obj_ids[1:]

#        ## split the color-encoded mask into a set
#        ## of binary masks
#        #masks = mask == obj_ids[:, None, None]
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ Shape: {masks.shape}')
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ masks: {masks}')

#        # get bounding box coordinates for each mask
#        #num_objs = len(obj_ids)
#        num_objs = masks.shape[0]
#        boxes = []
#        for i in range(num_objs):
#            pos = np.where(masks[i])
#            xmin = np.min(pos[1])
#            xmax = np.max(pos[1])
#            ymin = np.min(pos[0])
#            ymax = np.max(pos[0])
#            boxes.append([xmin, ymin, xmax, ymax])

#        # convert everything into a torch.Tensor
#        boxes = torch.as_tensor(boxes, dtype=torch.float32)

#        if self.num_classes == 1:
#            labels = torch.ones((num_objs,), dtype=torch.int64) # For use with models with only 1 class
#        else:
#            labels = torch.as_tensor(mask_labels, dtype=torch.int64) # For use with multi-class models

#        #print(f'image {idx} path: {img_path}')
#        #print(f'image {idx} labels: {labels}')
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ labels: {labels.shape}')
#        masks = torch.as_tensor(masks, dtype=torch.uint8)

#        image_id = torch.tensor([idx])
#        area = (boxes[:, 3] - boxes[:, 1]) * (boxes[:, 2] - boxes[:, 0])
#        # suppose all instances are not crowd
#        iscrowd = torch.zeros((num_objs,), dtype=torch.int64)

#        target = {}
#        target["boxes"] = boxes
#        target["labels"] = labels
#        target["masks"] = masks
#        target["image_id"] = image_id
#        target["area"] = area
#        target["iscrowd"] = iscrowd


#        if self.transforms is not None:
#            img, target = self.transforms(img, target)

#        return img, target

#        ##############################################################################
#        ## load images and masks
#        #img_path = os.path.join(self.root, "DatasetImages", self.imgs[idx])
#        #mask_path = os.path.join(self.root, "DatasetMasks", self.masks[idx])
#        #img = Image.open(img_path).convert("RGB")
#        ## note that we haven't converted the mask to RGB,
#        ## because each color corresponds to a different instance
#        ## with 0 being background
#        #mask = Image.open(mask_path)
#        ## convert the PIL Image into a numpy array
#        #mask = np.array(mask)
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ Shape: {mask.shape}')

#        ##print(f"&&&&&&&&&&&& Shape: {mask.shape}")
#        ## instances are encoded as different colors
#        #obj_ids = np.unique(mask)
#        ##print(f"****************************** image: {self.imgs[idx]}")
#        ##print(f"****************************** mask: {self.masks[idx]}")
#        ##print(f"****************************************************************Unique IDs for image {idx}: {obj_ids}")

#        ## first id is the background, so remove it
#        #obj_ids = obj_ids[1:]

#        ## split the color-encoded mask into a set
#        ## of binary masks
#        #masks = mask == obj_ids[:, None, None]
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ Shape: {masks.shape}')
#        #print(f'\t\t$$$$$$$$$$$$$$$$$ masks: {masks}')

#        ## get bounding box coordinates for each mask
#        #num_objs = len(obj_ids)
#        #boxes = []
#        #for i in range(num_objs):
#        #    pos = np.where(masks[i])
#        #    xmin = np.min(pos[1])
#        #    xmax = np.max(pos[1])
#        #    ymin = np.min(pos[0])
#        #    ymax = np.max(pos[0])
#        #    boxes.append([xmin, ymin, xmax, ymax])

#        ## convert everything into a torch.Tensor
#        #boxes = torch.as_tensor(boxes, dtype=torch.float32)
#        ## there is only one class
#        #labels = torch.ones((num_objs,), dtype=torch.int64)
#        #masks = torch.as_tensor(masks, dtype=torch.uint8)

#        #image_id = torch.tensor([idx])
#        #area = (boxes[:, 3] - boxes[:, 1]) * (boxes[:, 2] - boxes[:, 0])
#        ## suppose all instances are not crowd
#        #iscrowd = torch.zeros((num_objs,), dtype=torch.int64)

#        #target = {}
#        #target["boxes"] = boxes
#        #target["labels"] = labels
#        #target["masks"] = masks
#        #target["image_id"] = image_id
#        #target["area"] = area
#        #target["iscrowd"] = iscrowd

#        #if self.transforms is not None:
#        #    img, target = self.transforms(img, target)

#        #return img, target
#        ###############################################################
#        ## load images and masks
#        #img_path = os.path.join(self.root, "DatasetImages", self.imgs[idx])
#        #mask_path = os.path.join(self.root, "DatasetMasks", self.masks[idx])
#        #img = Image.open(img_path).convert("RGB")

#        ## note that we haven't converted the mask to RGB,
#        ## because each color corresponds to a different instance
#        ## with 0 being background
#        #mask = Image.open(mask_path)

#        ## convert the PIL Image into a numpy array
#        #mask = np.array(mask)

#        #print(f"****************************************************************mask shape for {idx}: {mask.shape}")

#        ## instances are encoded as different colors
#        #obj_ids = np.unique(mask)
#        #print(f"****************************************************************Unique IDs for image {idx}: {obj_ids}")
#        #print(f"****************************************************************Unique IDs shape for image {idx}: {obj_ids.shape}")

#        ## first id is the background, so remove it
#        #obj_ids = obj_ids[1:]

#        ### split the color-encoded mask into a set
#        ### of binary masks
#        ##test_obj = obj_ids[:, None, None]
#        ##print(f"****************************************************************Test Obj shape for image {idx}: {test_obj.shape}")
#        ##print(f"****************************************************************Test Obj {idx}: {test_obj}")

#        #masks = mask == obj_ids[:, None, None] # A = B == C [3, 585, 595] [2]

#        ############### Our mask has the channels at the last position in the shape, we want it to be the first position. So reshape the mask
#        ##mask = np.reshape(mask, (mask.shape[2], mask.shape[0], mask.shape[1]))


#        ##print(f"****************************************************************mask shape for {idx}: {mask.shape}")
#        ##print(f"****************************************************************first channel shape: {idx}: {mask[0].shape}")

#        ##onecount = 0
#        ##twocount = 0
#        ##threecount = 0

#        ##s = mask[0].shape
#        ##for a in range(s[0]):
#        ##    for b in range(s[1]):
#        ##        if mask[0][a][b] == 1:
#        ##            onecount += 1
#        ##        elif mask[0][a][b] == 2:
#        ##            twocount += 1
#        ##        elif mask[0][a][b] == 3:
#        ##            threecount += 1

#        ##print(f"****************************************************************one count: {onecount}")
#        ##print(f"****************************************************************two count: {twocount}")
#        ##print(f"****************************************************************three count: {threecount}")

#        ##masks = []
#        ##for id in obj_ids:
#        ##    curr_mask = mask[0] == id
#        ##    masks.append(curr_mask)

#        ##masks = np.array(masks)

#        ##print(f"****************************************************************Unique IDs shape for image {idx}: {obj_ids.shape}")
#        ##print(f"****************************************************************masks shape for {idx}: {masks.shape}")
#        ##print(f"****************************************************************masks for {idx}: {masks}")
#        ##print(f"****************************************************************any trues? {idx}: {np.any(masks[0])}")
#        ##print(f"****************************************************************any trues? {idx}: {np.any(masks[1])}")
#        ##print(f"****************************************************************any trues? {idx}: {np.any(masks[2])}")

#        ##sumval = np.sum(masks[0])
#        ##print(f"****************************************************************sum of trues? {idx}: {sumval}")
#        ###im = Image.fromarray(masks[0])
#        ###threshold = 0
#        ###im = im.point(lambda p: p > threshold and 255)
#        ###im.show()


#        ## get bounding box coordinates for each mask
#        #num_objs = len(obj_ids)
#        #boxes = []
#        #for i in range(num_objs):

#        #    pos = np.where(masks[i])
#        #    print(f"        ****************************************************************pos for {idx}: {pos}")
#        #    xmin = np.min(pos[1])
#        #    xmax = np.max(pos[1])
#        #    ymin = np.min(pos[0])
#        #    ymax = np.max(pos[0])
#        #    boxes.append([xmin, ymin, xmax, ymax])

#        ## convert everything into a torch.Tensor
#        #boxes = torch.as_tensor(boxes, dtype=torch.float32)
#        ## there is only one class
#        #labels = torch.ones((num_objs,), dtype=torch.int64)
#        #print(f"****************************************************************labels for {idx}: {labels}")
#        #masks = torch.as_tensor(masks, dtype=torch.uint8)

#        #image_id = torch.tensor([idx])
#        #area = (boxes[:, 3] - boxes[:, 1]) * (boxes[:, 2] - boxes[:, 0])
#        ## suppose all instances are not crowd
#        #iscrowd = torch.zeros((num_objs,), dtype=torch.int64)

#        #target = {}
#        #target["boxes"] = boxes
#        #target["labels"] = labels
#        #target["masks"] = masks
#        #target["image_id"] = image_id
#        #target["area"] = area
#        #target["iscrowd"] = iscrowd

#        #if self.transforms is not None:
#        #    img, target = self.transforms(img, target)

#        #return img, target

#    def __len__(self):
#        return len(self.imgs)



""" Helper function for finetuning from a pretrained model """

import torchvision
from torchvision.models.detection.faster_rcnn import FastRCNNPredictor
from torchvision.models.detection.mask_rcnn import MaskRCNNPredictor


from torchvision.models.detection import MaskRCNN
from torchvision.models.detection.anchor_utils import AnchorGenerator


def get_resnet50_model_instance_segmentation(num_classes, model_load_dir=None):
    ############################## METHOD FROM PYTORCH MASK RCNN TUTORIAL ######################################
    num_classes = num_classes + 1 # Number of predicted classes + 1 background class

    # load a model pre-trained pre-trained on COCO
    model = torchvision.models.detection.maskrcnn_resnet50_fpn(pretrained=True)

    # get number of input features for the classifier (I think this is a property of the pre-trained model)
    in_features = model.roi_heads.box_predictor.cls_score.in_features

    # replace the pre-trained head with a new one
    model.roi_heads.box_predictor = FastRCNNPredictor(in_features, num_classes)

    # now get the number of input features for the mask classifier
    in_features_mask = model.roi_heads.mask_predictor.conv5_mask.in_channels
    hidden_layer = 256

    # and replace the mask predictor with a new one
    model.roi_heads.mask_predictor = MaskRCNNPredictor(in_features_mask,
                                                       hidden_layer,
                                                       num_classes)
    
    if model_load_dir is not None:
        model.load_state_dict(torch.load(model_load_dir))

    #print(f"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Model: {model}")
    #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Model dict: {model.__dict__}")

    # That's it, this will make model be ready to be trained and evaluated on your custom dataset.
    return model


def get_mobilenetv2_model_instance_segmentation(num_classes):
    ############################## METHOD FROM mask_rcnn.py in Pytorch site-packages (attempting to use different backbone) ######################################
    num_classes = num_classes + 1 # Number of predicted classes + 1 background class
    # load a pre-trained model for classification and return
    # only the features
    backbone = torchvision.models.mobilenet_v2(pretrained=True).features
    # MaskRCNN needs to know the number of
    # output channels in a backbone. For mobilenet_v2, it's 1280
    # so we need to add it here
    backbone.out_channels = 1280

    # let's make the RPN generate 5 x 3 anchors per spatial
    # location, with 5 different sizes and 3 different aspect
    # ratios. We have a Tuple[Tuple[int]] because each feature
    # map could potentially have different sizes and
    # aspect ratios
    anchor_generator = AnchorGenerator(sizes=((32, 64, 128, 256, 512),),
                                        aspect_ratios=((0.5, 1.0, 2.0),))

    # let's define what are the feature maps that we will
    # use to perform the region of interest cropping, as well as
    # the size of the crop after rescaling.
    # if your backbone returns a Tensor, featmap_names is expected to
    # be ['0']. More generally, the backbone should return an
    # OrderedDict[Tensor], and in featmap_names you can choose which
    # feature maps to use.
    roi_pooler = torchvision.ops.MultiScaleRoIAlign(featmap_names=['0'],
                                                    output_size=7,
                                                    sampling_ratio=2)

    mask_roi_pooler = torchvision.ops.MultiScaleRoIAlign(featmap_names=['0'],
                                                         output_size=14,
                                                         sampling_ratio=2)
    # put the pieces together inside a MaskRCNN model
    model = MaskRCNN(backbone,
                     num_classes=num_classes,
                     rpn_anchor_generator=anchor_generator,
                     box_roi_pool=roi_pooler,
                     mask_roi_pool=mask_roi_pooler)

    return model

""" Helper function for data augmentation / transformation """



#def get_transform(train):
#    transforms = []
#    # converts the image, a PIL image, into a PyTorch Tensor
#    transforms.append(T.ToTensor())
#    if train:
#        # during training, randomly flip the training images
#        # and ground-truth for data augmentation
#        #transforms.append(T.RandomHorizontalFlip(0.5)) # commented this because we are performing our own augmentations prior to training.
#        pass
#    return T.Compose(transforms)


#def train_model(dataset_root, model_save_dir, model = None, num_classes=1, dataset_test_root=None, dataset_split=0.3, train_batch_size=2, test_batch_size=1, num_epochs=10, backbone='resnet50'):
#    ##################### SET UP AND TRAIN MODEL ###########################

#    # train on the GPU or on the CPU, if a GPU is not available
#    device = torch.device('cuda') if torch.cuda.is_available() else torch.device('cpu')

#    #num_classes = 1 + num_classes # Background class + our classes
#    if dataset_test_root is None:
#        dataset_test_root = dataset_root
    
#    # use our dataset and defined transformations
#    dataset = WormDataset(dataset_root, get_transform(train=True), num_classes=num_classes)
#    dataset_test = WormDataset(dataset_test_root, get_transform(train=False), num_classes=num_classes)

#    print(f"Dataset length: {len(dataset)}")
#    print(f"Dataset test length: {len(dataset_test)}")

#    if dataset_test_root is None:
#        test_size = int(len(dataset) * dataset_split) # 70% of dataset for training, 30% for testing
#        #test_size = 6

#        # split the dataset in train and test set
#        indices = torch.randperm(len(dataset)).tolist()
#        dataset = torch.utils.data.Subset(dataset, indices[:-1*test_size])
#        dataset_test = torch.utils.data.Subset(dataset_test, indices[-1*test_size:])

#        print(f"Dataset length: {len(dataset)}")
#        print(f"Dataset test length: {len(dataset_test)}")
    
#    #train_batch_size = 4
#    #test_batch_size = 2
#    #train_batch_size = 2
#    #test_batch_size = 1

#    # define training and validation data loaders
#    data_loader = torch.utils.data.DataLoader(
#        dataset, batch_size=train_batch_size, shuffle=True, num_workers=4,
#        collate_fn=U.collate_fn)

#    data_loader_test = torch.utils.data.DataLoader(
#        dataset_test, batch_size=test_batch_size, shuffle=False, num_workers=4,
#        collate_fn=U.collate_fn)

#    # get the model using our helper function
#    if model is None:
#        print(' -------------------------- Training has fetched a new model')
#        assert backbone in ['resnet50', 'mobilenetv2']
#        if backbone == 'resnet50':
#            model = get_resnet50_model_instance_segmentation(num_classes)
#        else:
#            model = get_mobilenetV2_model_instance_segmentation(num_classes)
#    else:
#        print(' -------------------------- Training from passed model')

#    # move model to the right device
#    model.to(device)

#    # construct an optimizer
#    params = [p for p in model.parameters() if p.requires_grad]
#    optimizer = torch.optim.SGD(params, lr=0.005,
#                                momentum=0.9, weight_decay=0.0005)
#    # and a learning rate scheduler which decreases the learning rate by
#    # 10x every 3 epochs
#    lr_scheduler = torch.optim.lr_scheduler.StepLR(optimizer,
#                                                   step_size=3,
#                                                   gamma=0.1)

    

#    if not os.path.exists(model_save_dir):
#        print("&&&&&&&&&&&&&&&&&&&& MAKING DIR &&&&&&&&&&&&&&&&&&&&&&&&&&")
#        os.mkdir(model_save_dir)

    
#    obj_now = datetime.now()
#    # formatting the time
#    current_time = obj_now.strftime("%Y-%m-%d_%H-%M-%S")
#    model_filename = f'model_' + current_time + f'_{backbone}' + f'_{num_classes}classes' + f'_{num_epochs}epochs'


#    # Open a file that we will use to save the training data over time.
#    training_data_filename = os.path.join(model_save_dir, model_filename + "_TrainingData.txt")
#    training_data = open(training_data_filename, "w")
#    training_data.write('PRECISION Metric for BBox and Segmentation:\n\tAverage Precision (AP) @[ IoU=0.75 | area=all | maxDets=100 ]\n')
#    training_data.write('RECALL Metric for BBox and Segmentation:\n\tAverage Recall (AR) @[ IoU=0.50:0.95 | area=all | maxDets=100 ]\n\n')
#    training_data.write(f'Dataset has been split with {100 - (dataset_split*100)}% for training and {dataset_split*100}% for testing.\n')
#    training_data.write(f'Dataset Size: {len(dataset) + len(dataset_test)} ({len(dataset)} Training + {len(dataset_test)} Testing)\n')
#    training_data.write(f'Training Batch Size: {train_batch_size}, Testing Batch Size: {test_batch_size}\n')
#    training_data.write(f'Intending to train for {num_epochs} epochs.\n')
#    training_data.write("\nRESULTS:\nEpoch\tTime\t\t\tBBox Precision\t\tBBox Recall\t\tSegm Precision\t\tSegm Recall\n")


#    for epoch in range(num_epochs):
#        # train for one epoch, printing every 10 iterations
#        #print("Train one epoch inputs:")
#        #print(f"model: {model}")
#        #print(f"optimizer: {optimizer}")
#        #print(f"data loader: {data_loader}")
#        #print(f"device: {device}")
#        #print(f"epoch: {epoch}")

#        #print(f"Len dataloader: {len(data_loader)}")
#        #print("Done with parameters")
#        E.train_one_epoch(model, optimizer, data_loader, device, epoch, print_freq=10)
#        # update the learning rate
#        lr_scheduler.step()
#        # evaluate on the test dataset
#        coco_evaluator = E.evaluate(model, data_loader_test, device=device)
#        bbox_eval_results = coco_evaluator.coco_eval['bbox'].stats
#        segm_eval_results = coco_evaluator.coco_eval['segm'].stats
#        print(f"!!!!!!!!!!!!!! {bbox_eval_results}")
#        print(f"!!!!!!!!!!!!!! {bbox_eval_results[0]}")
#        bbox_acc = bbox_eval_results[2]
#        bbox_rec = bbox_eval_results[8]
#        segm_acc = segm_eval_results[2]
#        segm_rec = segm_eval_results[8]

#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {bbox_acc:<07.5}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {bbox_acc:.3}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {bbox_rec:.5}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {bbox_rec:.3}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {segm_acc:.5}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {segm_acc:.3}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {segm_rec:.5}")
#        #print(f"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ bbox acc: {segm_rec:.3}")

#        # Write the epoch's accuracy data to the text file
#        now_time_obj = datetime.now()
#        now_time = now_time_obj.strftime("%Y-%m-%d_%H-%M-%S")

#        training_data.write(f'{epoch+1}\t{now_time}\t{bbox_acc:.5f}\t\t\t{bbox_rec:.5f}\t\t\t{segm_acc:.5f}\t\t\t{segm_rec:.5f}\n')

#        # Save current checkpoint and evaluate accuracy
#        print("################# Saving Checkpoint ####################")
#        checkpoint_filename = model_filename + f'_{epoch+1}Done.pth'
#        checkpoint_file_path = os.path.join(model_save_dir, checkpoint_filename)
#        torch.save(model.state_dict(), checkpoint_file_path)

#    print("###################### Training done! ######################")
#    training_data.close()
#    model_save_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN'

#    if not os.path.exists(model_save_dir):
#        print("&&&&&&&&&&&&&&&&&&&& MAKING DIR &&&&&&&&&&&&&&&&&&&&&&&&&&")
#        os.mkdir(model_save_dir)

#    #obj_now = datetime.now()
#    ## formatting the time
#    #current_time = obj_now.strftime("%Y-%m-%d_%H-%M-%S")
#    #model_filename = f'model_' + current_time + f'_{num_epochs}epochs.pth'
#    ##model_file_path = model_save_dir + "\\" + model_filename
#    #model_file_path = os.path.join(model_save_dir, model_filename)

#    #print(model_file_path)

#    #torch.save(model.state_dict(), model_file_path)


def test_model(dataset_root, model, num_classes, device, batch_size=2):
    device = torch.device('cuda') if torch.cuda.is_available() else torch.device('cpu')
    
    class_params = U.get_labeling_params(exclude=['Egg'])

    dataset_test = WormDataset(dataset_root, get_transform(train=False), num_classes=num_classes, class_parameters=class_params)

    print(f"Dataset test length: {len(dataset_test)}")

    data_loader_test = torch.utils.data.DataLoader(
        dataset_test, batch_size=batch_size, shuffle=False, num_workers=4,
        collate_fn=U.collate_fn)

    # Move model to the right device
    model.to(device)

    coco_evaluator = E.evaluate(model, data_loader_test, device=device)

def predict_with_model(imgs, model, device = None):
    ################ LOAD A PREVIOUS MODEL AND TEST IT ####################

    if device is None:
        device = torch.device('cuda') if torch.cuda.is_available() else torch.device('cpu')
        print(f"Device was none. Selected: {device}")

    model.eval() # put the model in evaluation mode
    model.to(device)

    # Convert the image to a tensor if it has not already been converted
    for i in range(len(imgs)):
        if not isinstance(imgs[i], torch.Tensor):
            pil2tensor = torchvision.transforms.ToTensor()
            imgs[i] = pil2tensor(imgs[i])
        imgs[i] = imgs[i].to(device)
        
    with torch.no_grad():
        prediction = model(imgs)

    #print(f"Prediction: {prediction}")
    #print(f"Prediction size: {len(prediction)}")
    #print(f"Masks size: {len(prediction[0]['masks'])}")
    #print(f"Masks shape: {prediction[0]['masks'].shape}")
    #print(f"Masks: {prediction[0]['masks']}")
    #print(f"Box 0: {prediction[0]['boxes'][0]}")
    #print(f"Labels: {prediction[0]['labels']}")

    return prediction

def draw_masks_on_imgs(imgs, predictions, score_thresh=0.7, pix_conf_thresh = 0.5, label_type='stages', display_labels=True, orig_file_paths=[], include_orig_img = True):
    #boxes_and_masks_img = Image.fromarray(img.mul(255).permute(1, 2, 0).byte().numpy())
    images_with_masks = []
    for idx, prediction in enumerate(predictions):
        #print(f"Num predicted objects in img: {len(prediction['scores'])}")
            

        #boxes_and_masks_img.show()
        #cv_boxes_and_masks_img = np.array(boxes_and_masks_img)
        cv_boxes_and_masks_img_orig = np.array(imgs[idx])
        #print(f'test image shape: {cv_boxes_and_masks_img_orig.shape}')
        cv_boxes_and_masks_img = cv_boxes_and_masks_img_orig[:, :, ::-1].copy() 
        #print(f'test image shape: {cv_boxes_and_masks_img.shape}')


        #test_image_mask = Image.fromarray(prediction['masks'][0, 0].mul(255).byte().cpu().numpy())
        #test_image_mask.show()

        #test_image_mask = random_colour_masks(test_image_mask)
        #np.set_printoptions(threshold=sys.maxsize)
        #torch.set_printoptions(profile="full")

        #test_image_mask5 = Image.fromarray(prediction['masks'][0, 0].mul(255).byte().cpu().numpy())
        #test_image_mask5.show()
        #print(f"pred masks: {prediction['masks'].shape}")
        #print(f"pred masks shape: {prediction['masks'][0, 0].shape}") 

        if label_type == 'worm':
            labeling_params = U.get_labeling_params()
            #labeling_params = U.get_gender_labeling_params()
            # If we're only classifying one class then replace all the labels with 'Worm'
            for id in labeling_params:
                labeling_params[id][0] = 'Worm'
        elif label_type == 'gender':
            labeling_params = U.get_gender_labeling_params()
        else:
            labeling_params = U.get_labeling_params() # Stages

        #score_thresh = 0.7

        # Visualize the predictions
        num_masks = prediction['masks'].shape[0] # The shape of masks is (N, 1, H, W) where N is the number of masks for the given image
        #print(f'shape of masks: {prediction["masks"].shape[0]}')
        for n in range(num_masks): 
            # If the prediction is below the confidence threshold then ignore it
            if prediction['scores'][n] <= score_thresh:
                continue

            #If the prediction is on the GPU, we must move it to the CPU so we can convert it to numpy
            prediction['masks'] = (prediction['masks']).cpu()

            # Draw the current mask on the image
            this_mask = np.array(prediction['masks'][n, 0])
            this_mask = (this_mask > pix_conf_thresh).astype(np.uint8)
            this_mask = this_mask * 255
            mask_pixel_area = cv2.countNonZero(this_mask)
            #print(f'mask pixel area: {mask_pixel_area}')
            this_mask = cv2.cvtColor(this_mask,cv2.COLOR_GRAY2RGB)
            this_mask, color = random_colour_masks(this_mask)
            #img_w_mask = cv2.addWeighted(cv_boxes_and_masks_img, 1, this_mask, 0.5, 0.0)
            #print(f'sizes: {cv_boxes_and_masks_img.shape} , {this_mask.shape}')
            cv_boxes_and_masks_img = cv2.addWeighted(cv_boxes_and_masks_img, 1, this_mask, 0.5, 0.0) # Draws the transparent mask on top of the image

            # Draw the box around the mask
            top_left_of_box = (int((prediction['boxes'][n][0]).item()), int((prediction['boxes'][n][1]).item()))
            bottom_right_of_box = (int((prediction['boxes'][n][2]).item()), int((prediction['boxes'][n][3]).item()))
            cv_boxes_and_masks_img = cv2.rectangle(cv_boxes_and_masks_img, top_left_of_box, bottom_right_of_box, color, 2)

            #print(f"top left box: {top_left_of_box}")
            #print(f" label n : {prediction['labels'][n]}")
            #print(f" label n type : {(prediction['labels'][n]).item()}")
            #print(f" label n : {labeling_params[(prediction['labels'][n]).item()]}")
            #print(f" label n : {labeling_params[(prediction['labels'][n]).item()][0]}")

            if display_labels:
                # Write the mask's label at the top of it's box
                label_text = labeling_params[(prediction['labels'][n]).item()][0]
                cv_boxes_and_masks_img = cv2.putText(cv_boxes_and_masks_img, label_text ,(top_left_of_box[0], top_left_of_box[1] - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 2)

        if orig_file_paths:
            cv_boxes_and_masks_img_orig = cv2.putText(cv_boxes_and_masks_img_orig, orig_file_paths[idx] ,(5, 15), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,0,0), 2)

        #cv2.imshow('opencv mask', cv_boxes_and_masks_img_mask)
        #cv2.waitKey(0)
    
        #print(f'test mask shape: {cv_boxes_and_masks_img_mask.shape}')
        #print(f'test mask: {np.where(cv_boxes_and_masks_img_mask>0)}')
        #print(f'length: {len(cv_boxes_and_masks_img_mask)}')


        #print(f'test img type: {cv_boxes_and_masks_img.dtype}')
        #print(f'test img mask type: {this_mask.dtype}')
        #img_w_mask = cv2.addWeighted(cv_boxes_and_masks_img, 1, cv_boxes_and_masks_img_mask, 0.5, 0.0)
        #cv2.imshow("masked image", img_w_mask)
        if include_orig_img:
            cv_boxes_and_masks_img = np.concatenate((cv_boxes_and_masks_img_orig, cv_boxes_and_masks_img), axis=1)
        
        images_with_masks.append(cv_boxes_and_masks_img)

    #for masked_img in images_with_masks:
    #    cv2.imshow("masked image", masked_img)
    #    cv2.waitKey(0)
    return images_with_masks


def gather_stage_length_data(dataset_root, model_dir, num_dataset_classes, num_model_classes):
    # Get the Dataset
    exclude = ['Egg', 'Unknown-Stage']
    class_params = U.get_labeling_params()
    class_params = {x : class_params[x] for x in class_params if class_params[x][0] not in exclude} # Filter out the unwanted labels
    dataset = WormDataset(dataset_root, get_transform(train=False), num_classes=num_dataset_classes, class_parameters=class_params)

    # Use the GPU if available. 
    #device = torch.device('cuda') if torch.cuda.is_available() else torch.device('cpu')

    # get the model using our helper function
    model = get_resnet50_model_instance_segmentation(num_model_classes)
    model.load_state_dict(torch.load(model_dir))
    model.eval() # put the model in evaluation mode

    # Open a file for writing the staging data
    obj_now = datetime.now()
    current_time = obj_now.strftime("%Y-%m-%d_%H-%M-%S") # formatting the time

    staging_filename = f'StagingData_' + current_time + f'.txt'
    stage_length_data_filename = os.path.join(dataset_root, staging_filename)
    stage_length_data = open(stage_length_data_filename, "w")
    stage_length_data.write('Staging Length Data Recording\n')
    stage_length_data.write(f'Model used: {model_dir}\n')
    stage_length_data.write(f'Mask area is in pixels.\n\n')
    stage_length_data.write("\nDATA:\nImg ID\tLabel\tPred Mask Area\tLabel Mask Area\n")

    not_one_worm = 0
    edge_worms = 0
    low_pred_score = 0
    
    for image_idx in range(dataset.__len__()):
        # Get the labeled mask
        img, target = dataset.__getitem__(image_idx)
        #print(f'{image_idx}:\n{target}')
        
        # For simplicity, we are only interested in images with a single object
        # This way we need not worry about correlating which predicted mask corresponds to which labeled mask.
        if len(target['labels']) != 1:
            print(f'Image skipped - ID: {image_idx} - Image does not have only 1 worm.')
            not_one_worm += 1
            continue

        # Ignore images with the worm too close to the edge, we do this because we are interested in 
        # the total length of the worm, so we don't want the worm to be cut off by the edge of the image
        # I arbitrarily set 20 pixels from the edge as the cut off point, but I think our hand masking is more than 
        # accurate enough that if the box is more than 20 pixels from the edge then the actual worm does not touch the edge.
        edge_pixels_thresh = 20
        bbox_xmin = target['boxes'][0][0]
        bbox_ymin = target['boxes'][0][1]
        bbox_xmax = target['boxes'][0][2]
        bbox_ymax = target['boxes'][0][3]
        mask_shape = (target['masks'][0].shape)
        if bbox_xmin < edge_pixels_thresh or bbox_ymin < edge_pixels_thresh or bbox_xmax > mask_shape[1] - edge_pixels_thresh or bbox_ymax > mask_shape[0] - edge_pixels_thresh:
            print(f'Image skipped - ID: {image_idx} - worm too close to edge.')
            edge_worms += 1
            continue

        # Get the model's prediction
        score_thresh = 0.7
        print(f'Getting prediction for image id: {image_idx}')
        prediction = predict_with_model([img], model)
        print('Prediction received')

        # Ignore images with low prediction confidence
        if prediction[0]['scores'][0] < score_thresh:
            print(f'Image skipped - ID: {image_idx} - Prediction score too low.')
            low_pred_score += 1
            continue
        
        #If the prediction is on the GPU, we must move it to the CPU so we can convert it to numpy
        prediction[0]['masks'] = (prediction[0]['masks']).cpu()

        # Get the area of the predicted mask
        pred_mask = np.array(prediction[0]['masks'][0, 0])
        pred_mask = (pred_mask > 0.5).astype(np.uint8)
        pred_mask = pred_mask * 255
        pred_mask_pixel_area = cv2.countNonZero(pred_mask)
        #print(f'pred mask area: {pred_mask_pixel_area}')

        # Get the area of the labeled mask
        labeled_mask = np.array(target['masks'][0])
        labeled_mask_pixel_area = cv2.countNonZero(labeled_mask)
        #print(f'labeled mask area: {labeled_mask_pixel_area}')

        # Get the label
        this_label_id = target['labels'][0].item()
        label_params = U.get_labeling_params()
        this_label = label_params[this_label_id][0]
        
        stage_length_data.write(f"{image_idx}\t{this_label}\t{pred_mask_pixel_area}\t{labeled_mask_pixel_area}\n")

        print(f'{image_idx} Done\n')
        #print(target)
        #print(len(target['labels']))
        #print((target['masks'][0].shape)[0])
        #print(target['boxes'][0][0] > 250)
        #break
    print(f'Not one worm: {not_one_worm}, edge worms: {edge_worms}, low prediction score: {low_pred_score}')

    stage_length_data.close()

    #print(dataset.__getitem__(35))
    print(dataset.__len__())

def graph_staging_data(labels):
    stage_data_filepath = r'C:\Users\pdb69\Desktop\WormTrainingData\StagingLengthData\StagingDataToUse\Pred vs Labeled Mask Area - Sheet2.csv'

    import pandas as pd
    import matplotlib.pyplot as plt
    from numpy.polynomial.polynomial import polyfit
    staging_data = pd.read_csv(stage_data_filepath)

    data = []
    for label in labels:
        data.append(staging_data[staging_data['Label'] == label])

    #L1 = staging_data[staging_data['Label'] == 'L1-Larva']
    #L2 = staging_data[staging_data['Label'] == 'L2-Larva']
    #L3 = staging_data[staging_data['Label'] == 'L3-Larva']
    #Adu_Herm = staging_data[staging_data['Label'] == 'Adult-Herm']
    #Adu_Male = staging_data[staging_data['Label'] == 'Adult-Male']

    s = 15
    
    plts = []
    x_data = []
    y_data = []
    for stage in data:
        plts.append(plt.scatter(stage['Pred Mask Area'], stage['Label Mask Area'], s=s))
        for x in stage['Pred Mask Area']:
            x_data.append(x)
        for y in stage['Label Mask Area']:
            y_data.append(y)

    #print(f"==============================x: {x_data}")
    #print(f"==============================y: {y_data}")
    #print(f"==============================x len: {len(x_data)}")
    #print(f"==============================y len: {len(y_data)}")

    correlation_matrix = np.corrcoef(x_data, y_data)
    correlation_xy = correlation_matrix[0,1]
    r_squared = correlation_xy**2

    print(f"R Squared: {r_squared}")
    b, m = polyfit(x_data, y_data, 1)
    print(f"b: {b}, m: {m}")
    bisector_y = []
    for x in x_data:
        bisector_y.append((m*x) + b)

    plts.append(plt.plot(x_data, bisector_y, '--', color='black'))

    #plt_l1 = plt.scatter(L1['Pred Mask Area'], L1['Label Mask Area'], s=s)
    #plt_l2 = plt.scatter(L2['Pred Mask Area'], L2['Label Mask Area'], s=s)
    #plt_l3 = plt.scatter(L3['Pred Mask Area'], L3['Label Mask Area'], s=s)
    #plt_AHerm = plt.scatter(Adu_Herm['Pred Mask Area'], Adu_Herm['Label Mask Area'], s=s)
    #plt_AMale = plt.scatter(Adu_Male['Pred Mask Area'], Adu_Male['Label Mask Area'], s=s)

    #plt.legend([plt_l1, plt_l2, plt_l3, plt_AHerm, plt_AMale], ['L1-Larva', 'L2-Larva','L3-Larva','Adult-Herm','Adult-Male'])
    #plt.legend(plts, labels)
    #plt.title('Predicted vs Labeled Mask Area of Worm Stages')
    plt.xlabel('Machine Labeled Contour Area ($μm^2$)', fontsize=14)
    plt.ylabel('Human Labeled Contour Area ($μm^2$)', fontsize=14)
    plt.ticklabel_format(axis="x", style="sci", scilimits=(0,0))
    plt.ticklabel_format(axis="y", style="sci", scilimits=(0,0))

    plt.savefig(r'C:\Users\pdb69\Desktop\WormTrainingData\StagingLengthData\StagingData.png')

def predict_images_in_folders(model, device, score_thresh=0.7, label_type='stages'):
    from verifyDirlist import verifyDirlist
    import tkinter as tk
    import tkinter.filedialog

    root = tk.Tk()
    root.withdraw()

    pimages = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021'
    pfinal = r'C:\Users\pdb69\Desktop\WormTrainingData\ImagesWithPredictedMasks'

    pimages = tk.filedialog.askdirectory(parent=root, initialdir=pimages, title='Please select the folder containing the images for inference.')
    pfinal = tk.filedialog.askdirectory(parent=root, initialdir=pfinal, title='Please select the folder in which to place the new images.')

    folder_paths, folder_names = verifyDirlist(pimages,True)
    for idx,folder_path in enumerate(folder_paths):
        #if folder_names[idx] not in ['003', '004']:
        #    continue

        print(f'Predicting images in folder: {folder_names[idx]}')
        img_paths, img_names = verifyDirlist(folder_path,False,".png")
        images = []
        orig_file_paths = []
        for idx, img_path in enumerate(img_paths):
            #print(img_path)
            img = Image.open(img_path).convert("RGB")
            images.append(img)
            orig_file_paths.append(img_path.split('/')[-1])
            if idx % 10 == 0 or idx == len(img_paths) - 1:
                prediction = predict_with_model(images.copy(), model, device)
                #print(prediction)
                print(f'\tPredicted {idx + 1} of {len(img_paths)} images')
                masked_imgs = draw_masks_on_imgs(images, prediction, score_thresh=score_thresh, label_type=label_type, display_labels = True, orig_file_paths=orig_file_paths)

                for save_idx, masked_img in enumerate(masked_imgs):
                    #cv2.imshow("masked image", masked_img)
                    #cv2.waitKey(0)
                    this_img_path = os.path.join(pfinal, img_names[idx-(len(masked_imgs) - 1 - save_idx)])
                    #print(this_img_path)
                    cv2.imwrite(this_img_path, masked_img)


                images = []
                orig_file_paths = []
        



    

    


if __name__ == "__main__":

    ##### Training
    #dataset_root = r'C:\Users\antho\Downloads\PennFudanPed\PennFudanPed'
    #dataset_root = r'C:\Users\pdb69\Downloads\PennFudanPed\PennFudanPed'
    #dataset_root = r'C:\Dropbox\FramesData\FramesForAnnotation\train\Renamed'
    #dataset_root = r'C:\Users\pdb69\Desktop\WormTrainingData\MultiClassTraining'
    #dataset_root = r'C:\Users\pdb69\Desktop\WormTrainingData\StagingLengthData'
    dataset_root = r'C:\Users\lizih\Dropbox\UPenn_2021_Spring\Setup_test_dataset_to_delete'

    # Set up filepaths to save checkpoints after every epoch
    model_save_dir = r'C:\Users\lizih\Dropbox\UPenn_2021_Spring\Setup_test_dataset_to_delete'

    #num_model_classes = 8 # Number Predicted classes (this should exclude the background class, which is implied by the model)
    num_model_classes = 3 # Number Predicted classes (this should exclude the background class, which is implied by the model)
    num_epochs = 25

    #model_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN\ModelsToKeep\model_2021-04-26_22-23-06_50epochs_5Done.pth' # Multiclass model (8 classes) trained on limited dataset
   # model_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN\ModelsToKeep\model_2021-04-16_22-06-40_100epochs_15Done.pth' # Single Class model trained on Adult Herms and Males
    #model_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN\model_2021-04-29_23-27-44_resnet50_1classes_20epochs_20Done.pth' # Single class model trained on adults and then larva
    #model_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN\model_2021-05-02_11-09-39_resnet50_1classes_22epochs_17Done.pth' # Single class model trained on L1 - AHerm
    #model_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN\model_2021-05-04_18-14-19_resnet50_1classes_25epochs_20Done_DiscreteTrainTest_L1-AH.pth' # Single class model trained on L1 - AHerm with descrete testing dataset
    #model_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN\Gender Models\model_2021-06-14_17-58-14_resnet50_4classes_25epochs_18Done.pth' # Gender Model trained on 05-ADF-masked
    model_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\nets\Mask-RCNN Head-Tail-Gender\Combined_ADF-YF-ZL_WholeHeadMasks_Augmented\model_2021-06-15_16-06-17_resnet50_4classes_25epochs_10Done.pth' # Gender Model trained full Combined_ADF-YF-ZL_WholeHeadMasks_Augmented dataset



    #model = get_resnet50_model_instance_segmentation(num_model_classes) # Start from scratch
    model = get_resnet50_model_instance_segmentation(num_model_classes, model_load_dir=model_dir) # Continue training a previous model

    #class_params = U.get_pick_labeling_params()
    class_params = U.get_labeling_params()
    #train_model(dataset_root, model_save_dir, model=model, dataset_split=0.3, num_classes=num_classes, num_epochs=num_epochs, class_parameters = class_params)
    
    ##### Prediction
    #image_dir = r'C:\Dropbox\FramesData\Mask_RCNN_Normalized_Frames\Adult_Male\males_009\high_mag_frames\adult_males_009_61.png'
    #image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_Mix_Herm\001\2021-04-26 (09-08-29)_3.png'
    #image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_Mix_Herm\001\2021-04-26 (09-03-49)_0.png'
    #image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_Adu_Herm\002\2021-04-24 (10-00-36)_4.png' # 3 Adult Herms, 2 cut off by edge
    #image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_L2_Herm\002\2021-04-22 (10-38-39)_1.png' # 2 L2
    #image_dir1 = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_Adu_Herm\002\2021-04-24 (10-00-36)_3.png' # 2 Adult Herms
    #image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_Adu_Herm\002\2021-04-24 (10-00-36)_0.png'
    #image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_L4_Herm\002\2021-04-23 (10-10-16)_17.png'
    image_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\Mask_RCNN_Data_Apr_2021\04222021_Mix_Herm\005\2021-04-26 (10-01-06)_0.png'
    

    # Use the GPU if available. 
    device = torch.device('cuda') if torch.cuda.is_available() else torch.device('cpu')

    # Get the model using our helper function
    #model = get_resnet50_model_instance_segmentation(num_model_classes)
    #model.load_state_dict(torch.load(model_dir))
    #model = get_resnet50_model_instance_segmentation(num_model_classes, model_load_dir=model_dir)

    # Grab a totally new image from disk to make a prediction on
    img = Image.open(image_dir).convert("RGB") 
    #img1 = Image.open(image_dir1).convert("RGB")


    #prediction = predict_with_model([img], model, device)
    #masked_imgs = draw_masks_on_imgs([img], prediction, num_classes=num_model_classes, display_labels = False)

    #for masked_img in masked_imgs:
    #    cv2.imshow("masked image", masked_img)
    #    cv2.waitKey(0)

    num_dataset_classes = 8

    #gather_stage_length_data(dataset_root, model_dir, num_dataset_classes, num_model_classes)

    #graph_staging_data(['L1-Larva','L2-Larva','L3-Larva','L4-Herm','Adult-Herm'])

    predict_images_in_folders(model, num_model_classes, device)

    #test_model(dataset_test_root_L1, model, num_model_classes, device, batch_size=1)
    #print('\n\n======================================================================\n\n')
    #test_model(dataset_test_root_L2, model, num_model_classes, device, batch_size=1)
    #print('\n\n======================================================================\n\n')
    #test_model(dataset_test_root_L3, model, num_model_classes, device, batch_size=1)
    #print('\n\n======================================================================\n\n')
    #test_model(dataset_test_root_L4, model, num_model_classes, device, batch_size=1)
    #print('\n\n======================================================================\n\n')
    #test_model(dataset_test_root_AH, model, num_model_classes, device, batch_size=1)
    #print('\n\n======================================================================\n\n')





