"""
    To run MASK RCNN Training:

    First, augment your dataset using AugmentMaskRCNNTrainingData.py

    Modify Mask_RCNN_Train.py in ActivityAnalyzer/mask_rcnn:

    You must change a few lines in the if __name__ == "__main__" section at the bottom of the file.
    Obviously all line numbers are as they are in my version of the file, but yours should be the same or very close.
    If not then just use the variable names. 

    Line 184 - dataset_root: set your dataset_root path. This should be the folder containing the DatasetImages and DatasetMasks folders.
        For now ensure that every image in the images folder has a corresponding mask in the masks folder.
    Line 188 - model_save_dir: Set the directory in which you want to save the model that is created.	
    Line 190 - class_params: get the class parameters. The class parameters are in the utils.py file inside ActivityAnalyzer/mask_rcnn_utils.
        You can exclude labels from the returned parameters if you want, by passing an exclude list to the function:
            i.e. U.get_labeling_params(exclude=['Egg'])
    Line 192 - num_model_classes: the number of classes for the model to label. It should automatically be taken from the class parameters. It 
        should equal the total number of classes you want the model to identify (this total should exlude the background class, which is 
        implicit in the PyTorch model).
    Line 194 - num_epochs: set how many epochs you would like to train
    Line 196 - model_dir: if you are training from a previous model we have already created then put the path to it here. If not then leave commented.
    Line 198 or 199 - model: use 198 if you are training a brand new model, use 199 if you are training from a previous model.
    Line 203 - train_model: the dataset_split parameter details how much of the dataset will be used for testing.
        i.e. if there are 100 images and dataset_split is 0.3 then 70 will be used for training and 30 will be used for testing.


    Once you have set up all the parameters then you just run the Mask_RCNN_Train.py script (in your virtual/conda environment
    if you use one) and it will do the training. A checkpoint of the model is saved after every epoch so if you need to stop
    the training before it has completed then you can pick it back up where it left off by re-running the training and loading
    the checkpoint model as per lines 196 and 199.
"""


import os
import numpy as np
import torch
import cv2
from datetime import datetime
from Mask_RCNN_Dataset import WormDataset
from Tutorial_Mask_R_CNN_PyTorchOfficial import *

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

def get_transform(train):
    '''
    Performs transformations on the training dataset.
    As of now we are performing our own augmentations and saving them as new images in the dataset, however, if you wanted to have the model automatically perform augmentations on the training
    dataset (they would be generated during training and not saved to disk) then you can add them to the "if train" statement.
    '''
    transforms = []
    # converts the image, a PIL image, into a PyTorch Tensor
    transforms.append(T.ToTensor())
    if train:
        # during training, randomly flip the training images
        # and ground-truth for data augmentation
        #transforms.append(T.RandomHorizontalFlip(0.5)) # commented this because we are performing our own augmentations prior to training.
        pass
    return T.Compose(transforms)



def train_model(dataset_root, model_save_dir, model = None, num_classes=1, dataset_test_root=None, dataset_split=0.3, train_batch_size=2, test_batch_size=1, num_epochs=10, backbone='resnet50', dataset_class_params=None):
    '''
    This method will train a Pytorch Mask RCNN model
    Parameters:
        dataset_root: the root folder for the dataset. The root folder must contain folders named "DatasetImages" and "DatasetMasks", which contain the images and their corresponding masks respectively.
        model_save_dir: the directory into which you would like to save your model. Checkpoints after each epoch are saved as well as the final model after training completion.
        model: If you would like to train a previously trained model (transfer learning) you may pass it here. If this is not specified then the method will get a new PyTorch model, pretrained on the COCO dataset.
        num_classes: The number of classes your model must identify. (This is excluding the background class which is implicit in the PyTorch models) - i.e. if you only want to identify "Worm" then num classes is 1.
        dataset_test_root: You can specify a seperate folder for the testing dataset if you like. If so, the directory given by dataset_root is used solely for training and this directory is used solely for testing. It must have a "DatasetImages" and a "DatasetMasks" folder just like the dataset_root parameter. If this is not specified then the dataset provided in dataset_root is split into a training and testing dataset according to the dataset_split parameter.
        dataset_split: If dataset_test_root is not specified then the dataset_root is split into a testing and training dataset via this parameter. i.e. if 0.3 is specified then 30% of the dataset will be used for testing and 70% will be used for training.
        train_batch_size: the batch size for training. If you run into memory issues during training try lowering this along with the test_batch_size.
        test_batch_size: the batch size for testing. If you run into memory issues during training try lowering this along with the train_batch_size.
        num_epochs: the number of epochs for which you would like to train
        backbone: if getting a new PyTorch model from scratch then you can specify its backbone here. Options are "resnet50" and "mobilenetv2". resnet50 is recommended. 
        dataset_class_params: Labeling parameters for how to decode the mask images. Should be in the format: {ClassId: (label, start_val, end_val, channel)}. If no parameters are provided it will use the parameters defined in util.py "get_labeling_params" method.
    '''

    # train on the GPU or on the CPU, if a GPU is not available
    device = torch.device('cuda') if torch.cuda.is_available() else torch.device('cpu')

    num_classes = 1 + num_classes # Background class + our classes
    seperate_test_dataset = True
    if dataset_test_root is None:
        seperate_test_dataset = False
        dataset_test_root = dataset_root
    
    # use our dataset and defined transformations
    dataset = WormDataset(dataset_root, get_transform(train=True), num_classes=num_classes, class_parameters=dataset_class_params)
    dataset_test = WormDataset(dataset_test_root, get_transform(train=False), num_classes=num_classes, class_parameters=dataset_class_params)

    print(f"Dataset length: {len(dataset)}")
    print(f"Dataset test length: {len(dataset_test)}")

    if not seperate_test_dataset:
        test_size = int(len(dataset) * dataset_split) # 70% of dataset for training, 30% for testing
        #test_size = 6

        # split the dataset in train and test set
        indices = torch.randperm(len(dataset)).tolist()
        dataset = torch.utils.data.Subset(dataset, indices[:-1*test_size])
        dataset_test = torch.utils.data.Subset(dataset_test, indices[-1*test_size:])

        print(f"Dataset length after splitting: {len(dataset)}")
        print(f"Dataset test length after splitting: {len(dataset_test)}")
    

    # define training and validation data loaders
    data_loader = torch.utils.data.DataLoader(
        dataset, batch_size=train_batch_size, shuffle=True, num_workers=4,
        collate_fn=U.collate_fn)

    data_loader_test = torch.utils.data.DataLoader(
        dataset_test, batch_size=test_batch_size, shuffle=False, num_workers=4,
        collate_fn=U.collate_fn)

    # get the model using our helper function
    if model is None:
        print(' -------------------------- Training has fetched a new model')
        assert backbone in ['resnet50', 'mobilenetv2']
        if backbone == 'resnet50':
            model = get_resnet50_model_instance_segmentation(num_classes)
        else:
            model = get_mobilenetV2_model_instance_segmentation(num_classes)
    else:
        print(' -------------------------- Training from passed model')

    # move model to the right device
    model.to(device)

    # construct an optimizer
    params = [p for p in model.parameters() if p.requires_grad]
    optimizer = torch.optim.SGD(params, lr=0.005,
                                momentum=0.9, weight_decay=0.0005)
    # and a learning rate scheduler which decreases the learning rate by
    # 10x every 3 epochs
    lr_scheduler = torch.optim.lr_scheduler.StepLR(optimizer,
                                                   step_size=3,
                                                   gamma=0.1)

    

    if not os.path.exists(model_save_dir):
        print("&&&&&&&&&&&&&&&&&&&& MAKING DIR &&&&&&&&&&&&&&&&&&&&&&&&&&")
        os.mkdir(model_save_dir)

    
    obj_now = datetime.now()
    # formatting the time
    current_time = obj_now.strftime("%Y-%m-%d_%H-%M-%S")
    model_filename = f'model_' + current_time + f'_{backbone}' + f'_{num_classes}classes' + f'_{num_epochs}epochs'


    # Open a file that we will use to save the training data over time.
    training_data_filename = os.path.join(model_save_dir, model_filename + "_TrainingData.txt")
    training_data = open(training_data_filename, "w")
    training_data.write('PRECISION Metric for BBox and Segmentation:\n\tAverage Precision (AP) @[ IoU=0.75 | area=all | maxDets=100 ]\n')
    training_data.write('RECALL Metric for BBox and Segmentation:\n\tAverage Recall (AR) @[ IoU=0.50:0.95 | area=all | maxDets=100 ]\n\n')
    training_data.write(f'Dataset has been split with {100 - (dataset_split*100)}% for training and {dataset_split*100}% for testing.\n')
    training_data.write(f'Dataset Size: {len(dataset) + len(dataset_test)} ({len(dataset)} Training + {len(dataset_test)} Testing)\n')
    training_data.write(f'Training Batch Size: {train_batch_size}, Testing Batch Size: {test_batch_size}\n')
    training_data.write(f'Intending to train for {num_epochs} epochs.\n')
    training_data.write("\nRESULTS:\nEpoch\tTime\t\t\tBBox Precision\t\tBBox Recall\t\tSegm Precision\t\tSegm Recall\n")


    for epoch in range(num_epochs):
        # train for one epoch, printing every 10 iterations
        E.train_one_epoch(model, optimizer, data_loader, device, epoch, print_freq=10)
        # update the learning rate
        lr_scheduler.step()
        # evaluate on the test dataset
        coco_evaluator = E.evaluate(model, data_loader_test, device=device)
        bbox_eval_results = coco_evaluator.coco_eval['bbox'].stats
        segm_eval_results = coco_evaluator.coco_eval['segm'].stats
        bbox_acc = bbox_eval_results[2]
        bbox_rec = bbox_eval_results[8]
        segm_acc = segm_eval_results[2]
        segm_rec = segm_eval_results[8]


        # Write the epoch's accuracy data to the text file
        now_time_obj = datetime.now()
        now_time = now_time_obj.strftime("%Y-%m-%d_%H-%M-%S")

        training_data.write(f'{epoch+1}\t{now_time}\t{bbox_acc:.5f}\t\t\t{bbox_rec:.5f}\t\t\t{segm_acc:.5f}\t\t\t{segm_rec:.5f}\n')

        # Save current checkpoint and evaluate accuracy
        print("################# Saving Checkpoint ####################")
        checkpoint_filename = model_filename + f'_{epoch+1}Done.pth'
        checkpoint_file_path = os.path.join(model_save_dir, checkpoint_filename)
        torch.save(model.state_dict(), checkpoint_file_path)

    print("###################### Training done! ######################")
    training_data.close()
    model_save_dir = r'C:\Users\pdb69\Desktop\PyTorch_MaskRCNN'

    if not os.path.exists(model_save_dir):
        print("&&&&&&&&&&&&&&&&&&&& MAKING DIR &&&&&&&&&&&&&&&&&&&&&&&&&&")
        os.mkdir(model_save_dir)


if __name__ == "__main__":

    ##### Training
    dataset_root = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderDatasets\Current_Full_Gender_Dataset\Training_Dataset'
    dataset_test_root = r'C:\Users\pdb69\Desktop\WormTrainingData\GenderDatasets\Current_Full_Gender_Dataset\Testing_Dataset'

    # Set up filepaths to save checkpoints after every epoch
    model_save_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\nets\Mask-RCNN Head-Tail-Gender\Full_Gender_Dataset_With_New_Augmentations'

    class_params = U.get_gender_labeling_params() # We can modify the label parameters before passing them to training if we want.
    #num_model_classes = 1 # Number Predicted classes (this should exclude the background class, which is implied by the model)
    num_model_classes = len(class_params)
    print(f"Number of classes used in the model is {num_model_classes}.")
    num_epochs = 25

    #model_dir = r'C:\Users\pdb69\Dropbox\FangYenLab\FramesData\nets\Mask-RCNN Head-Tail-Gender\Combined_ADF-YF-ZL_WholeHeadMasks_Augmented\model_2021-06-20_15-10-50_resnet50_4classes_9epochs_7Done.pth' # Gender model trained on initial full gender dataset

    model = get_resnet50_model_instance_segmentation(num_model_classes) # Start from scratch
    #model = get_resnet50_model_instance_segmentation(num_model_classes, model_load_dir=model_dir) # Continue training a previous model

    

    train_model(dataset_root, model_save_dir, model=model, dataset_split=0.3, num_classes=num_model_classes, dataset_test_root=dataset_test_root, num_epochs=num_epochs, dataset_class_params=class_params)
    
    