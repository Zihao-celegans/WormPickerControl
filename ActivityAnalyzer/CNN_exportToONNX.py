"""
    Exporting a Pytorch model to ONNX
    https://pytorch.org/tutorials/advanced/super_resolution_with_onnxruntime.html

    In addition to torch, onnx runtime is also required
    pip install onnx onnxruntime    (can run from conda prompt, but its not on conda itself)





    *********************** WARNING *********************************


    DOES NOT RUN ON WINDOWS 7 - YOU WILL SEE AN ERROR ABOUT DYNAMIC_AXES

    MUST USE WINDOWS 10


    *****************************************************************








"""

# Some standard imports
import numpy as np
# from cv2 import cv2
import cv2


# Torch imports
from torch import nn
import torch.utils.model_zoo as model_zoo
import torch.onnx

# ONNX runtime
import onnx
import onnxruntime


# My imports 
from CNN_Helper import *
from RCNN_Helper import *

from torchvision.transforms import ToTensor

# Load the model

def exportONNX(f_pth, img_size = 20, ker_size = 3, net_structure = [ 3, 6, 16, 32, 200, 84, 2 ], use_rcnn = False):
    # [1, 6, 16, 32, 200, 84, 2]
    torch_model = loadCNN(f_pth,img_size,ker_size,net_structure)

        
    f_onnx = f_onnx=f_pth.replace(".pth",".onnx")

    # set the model to inference mode, some parameters behave differently vs training mode
    torch_model.eval()

    # Generate random data to feed to the model, so that the ONNX can 'trace' through it the various computations
    # Input data can be random but must be right size. In this case, a 1 - channel 20x20 image
    # Batch size can be one, but final model will accept larger batches too

    batch_size = 1
    x = torch.randn(batch_size, net_structure[0], img_size, img_size, requires_grad=True)
    
    #img_path= r'E:\CNN_Datasets\Worm_Motion_test\test\worm\169_worm_0136.png'
    #image = cv2.imread(img_path)
    #image = np.reshape(image,(3,image.shape[0],image.shape[1]))
    #image = image.astype(float)
    ### print(image.dtype)
    #image = cv2.normalize(image,np.zeros(image.shape),0,1,cv2.NORM_MINMAX)
    ##cv2.imshow("TEST", image)
    ##cv2.waitKey(0)
    ###print(image)

    ###cv2.imshow("TEST", image)
    ###cv2.waitKey(0);

    ## image = ToTensor()(image).unsqueeze(0) # unsqueeze to add artificial first dimension
    #image = torch.from_numpy(image)
    #image = torch.unsqueeze(image, 0)
    ### image = torch.from_numpy(image)

    #image = image.to(torch.float32)

    ##print(image.numpy())
    
    ##cv2.imshow("TEST", image.numpy()[0])
    ##cv2.waitKey(0)

    ##image = Variable(image)
    #print(image)
    torch_out = torch_model(x)

    # Export the model
    # Export the model
    torch.onnx.export(torch_model,               # model being run
                      x,                         # model input (or a tuple for multiple inputs)
                      f_onnx,   # where to save the model (can be a file or file-like object)
                      export_params=True,        # store the trained parameter weights inside the model file
                      opset_version=10,          # the ONNX version to export the model to
                      do_constant_folding=True,  # whether to execute constant folding for optimization
                      input_names = ['input'],   # the model's input names
                      output_names = ['type'], # the model's output names
                      dynamic_axes={'input' : {0 : 'batch_size'},    # variable lenght axes
                                    'output' : {0 : 'batch_size'}})


    # Import the model to make sure it worked and check it
    onnx_model = onnx.load(f_onnx)
    onnx.checker.check_model(onnx_model)

    # Import the model using ONNX runtime and make sure it reaches the same value as we calculated before exporting

    ort_session = onnxruntime.InferenceSession(f_onnx)

    def to_numpy(tensor):
        return tensor.detach().cpu().numpy() if tensor.requires_grad else tensor.cpu().numpy()

    # compute ONNX Runtime output prediction
    ort_inputs = {ort_session.get_inputs()[0].name: to_numpy(x)}
    ort_outs = ort_session.run(None, ort_inputs)

    # compare ONNX Runtime and PyTorch results
    np.testing.assert_allclose(to_numpy(torch_out), ort_outs[0], rtol=1e-03, atol=1e-05)
    print("Exported model has been tested with ONNXRuntime, and the result looks good!")

"""
    Exporting a Mask-RCNN model to ONNX
    https://pytorch.org/vision/stable/_modules/torchvision/models/detection/mask_rcnn.html

    Image size and batch size must be constant. 


"""

def exportMaskRCNNtoONNX(f_pth, img_size = (612,512)):
   
    # Load file at set to evaluation mode, some parameters behave differently vs training mode
    torch_model = loadRCNN(f_pth,2)
    #torch_model = loadMobileNetMaskRCNN(f_pth,2) for mobilenet
    f_onnx = f_onnx=f_pth.replace(".pth",".onnx")
    torch_model.eval()

    # Generate random data to feed to the model, so that the ONNX can 'trace' through it the various computations
    # Input data can be random but must be right size. In this case, a 1 - channel 20x20 image
    # Batch size can be one, but final model will accept larger batches too

    batch_size = 1
    N_channels = 3

    x = torch.randn(batch_size, N_channels, img_size[0], img_size[1]) # requires_grad = True

    # Test model on sample image before exporting
    predictions = torch_model(x)

    # Export
    torch.onnx.export(torch_model, x, f_onnx, opset_version = 11)

    # Test with OpenCV import (doesn't work at lesat on faster-RCNN)
    #net = cv2.dnn.readNetFromONNX(f_onnx)


    return


def testReadFromTorch(f_pth):
    net = cv2.dnn.readNetFromTorch(f_pth)
    pass


if __name__ == "__main__":
    f_pth = r'C:\Dropbox\WormPicker (shared data)\model_2021-04-07_16-05-03_3epochs.pth'
    f_pth = r'C:\Users\Yuchi\Downloads\Dropbox\model_2022-06-08_15-44-47_resnet50_4classes_25epochs_25Done.pth'
    #f_pth = r'C:\Dropbox\WormPicker (shared data)\model_2021-04-21_12-38-03_3epochs_3Done.pth' # mobilenet
    #exportONNX(f_pth,img_size=img_size)
    #exportMaskRCNNtoONNX(f_pth)
    testReadFromTorch(f_pth)

