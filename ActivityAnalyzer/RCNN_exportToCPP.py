"""
    Exporting PyTorch to TorchScript for use in C++    
    https://pytorch.org/tutorials/advanced/cpp_export.html
    

    DID NOT WORK AS OF 4/22/21
"""

# STEP 1: CONVERT TO TORCHSCRIPT
#
#   - Option 1: Tracing, mechanism where structure of model is captured by evaluating it using example inputs (like ONNX code)
#   - Option 2: Explicit annotation

import torch
import torchvision
from RCNN_Helper import *

# Load model
f_rcnn = r'C:\Dropbox\WormPicker (shared data)\model_2021-04-07_16-05-03_3epochs.pth'
f_out = f_rcnn.replace('.pth','_traced.pt') # Output pt file
img_size = (612,512)
model = loadRCNN(f_rcnn,2)

# generate random example data
x = torch.rand(1, 3, img_size[0], img_size[1])

# set evaluation mode
model.eval()
output2= model(x)


# Use torch.jit.trace to generate a torch.jit.ScriptModule via tracing.
traced_script_module = torch.jit.trace(model, x)

# Test output again
output3 = traced_script_module(torch.ones(1, 3, img_size[0], img_size[1]))
output3[0, :5]

# Serialize script module to file
traced_script_module.save("traced_resnet_model.pt")


