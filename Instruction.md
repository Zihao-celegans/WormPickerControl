# WormPickerControl V1.0 Instructions

## How to run WormPickerControl

### Step 1. Clone the WormPickerControl repository to your local repository

### Step 2. Edit Config files to fit your local computer

### Step 3. Write your custom scripts in WormPickerPythonScript.py

### Step 4. Run the script WormPickerPythonScript.py and WormPicker would be able to implement specific tasks


## Example scripts for the common C. elegans procedures reported in the literature: https://doi.org/10.1101/2022.11.18.517134
Adapted from WormPickerPythonWrapper/WormPickerPythonScript.py

```python
import WormPicker
import PlateIndex as pidx
import Phenotype as phty

def main():

    # Launch WormPicker to run in background and setup connection
    WP = WormPicker.WormPicker()
    WP.LaunchAndConnect()

    ###### SCRIPT ACTIONS START HERE ######
        
```

### Example 1. Pick multiple worms with specific phenotypes from one plate to another (Supplementary Fig. 6)
```python
    #(row and col) of source plate
    idx_src = pidx.PlateIndex(row = 0, col = 0)
    
    #(row and col) of destination plate
    idx_dest = pidx.PlateIndex(row = 0, col = 1)
    
    #(row and col) of intermediate picking plate
    idx_inter = pidx.PlateIndex(row = 1, col = 0)
    
    #number of worms want to transfer from source to destination
    num_worm = 10
    
    #desired phenotype of worms (Green)
    Phenotype = phty.Phenotype(gfp = phty.GFP_type.GFP)
    
    #strain name and generation number
    strain = "YX256"
    genNum = 1

    #Execute the task
    WP.PickNWorms(idx_src, idx_dest, idx_inter, 
    num_worm, Phenotype, strain, genNum)
```

### Example 2. Generate mating between two strains (Fig. 3-4)

```python
    #(row and col) of first source plate/strain
    idx_strain_1 = pidx.PlateIndex(row = 0, col = 0)
    
    #(row and col) of second source plate/strain
    idx_strain_2 = pidx.PlateIndex(row = 0, col = 1)
    
    #(row and col) of destination plate for mating
    idx_dest = pidx.PlateIndex(row = 1, col = 1)
    
    #(row and col) of intermediate picking plate
    idx_inter = pidx.PlateIndex(row = 1, col = 0)
    
    #Execute the task
    WP.CrossStrains(idx_strain_1, idx_strain_2, idx_dest, idx_inter)
```

### Example 3. Isolate individual worms to single plates (Fig. 5)
```python
    #(row and col) of source plate
    idx_src = pidx.PlateIndex(row = 1, col = 1)
    
    #(row and col) of intermediate picking plate
    idx_inter = pidx.PlateIndex(row = 1, col = 0)
    
    #number of worms to single
    num_worm = 10
    
    #rows of plates to single worms into 
    rows_dest = [0, 1]
    
    #Execute the task
    WP.SingleWorms(idx_src, idx_inter, num_worm, rows_dest)
```
### Example 4. Assay/screen for population phenotypes over a set of plates
#### 4.1 Screen for 100% Green progeny (Fig. 3)
```python
    # Screen five plates located in row 0
    for i in range(5):
    
      #(row and col) of the plate to be screened
      idx_plate = pidx.PlateIndex(row = 0, col = i)
      
      #number of worms want to screen
      num_worm = 20
      
      #desired phenotype of worms (Green)
      phenotype = phty.Phenotype(gfp = phty.GFP_type.GFP)
      
      #percentage threshold of unwanted animals found above which the screen can quit (for saving time),
      #i.e. if more than 70% non-Green were found, then quit screening the current plate and to screen next plate
      thresh = 0.7
      
      #Execute the task
      WP.ScreenOnePlate(idx_plate, num_worm, want_spec_phenotype = True, phenotype, thresh = 0.7)
```
#### 4.2 Assay percentages of Green and Red progeny (Fig. 4)

```python
     # Screen three plates located in row 0
     for i in range(3):
     
      #(row and col) of the plate to be screened
      idx_plate = pidx.PlateIndex(row = 0, col = i)
      
      #number of worms want to screen
      num_worm = 100
      
      #Execute the task
      WP.ScreenOnePlate(idx_plate, num_worm, check_GFP = True, check_RFP = True);
```

```python


    ###### SCRIPT ACTIONS END HERE ######

    # Exit the python script
    WP.Exit()

if __name__ == "__main__":
    main()
    
```
