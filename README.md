# ledcsv
Converts a 24-bit Bitmap image (.bmp) to a 320 line .csv file with RBG values intended for a HERA display.
    You can convert to this image format using Microsoft Paint

You will need to first compile the program using the command: make ledcsv

Then you can run the program using the command: ./ledcsv [image] [csv]

    [image] needs to be a 24-bit Bitmap image (.bmp)
    [csv] needs to be a .csv file name that will be overwritten or created after it runs
    
A scaled image temp.bmp will also be created in the current directory

****************************************************************

The program first takes the source image and scales it down to a 43x42 px version so that it will fit the model below.
![HERA model](https://user-images.githubusercontent.com/3085100/69560068-c0958680-0f70-11ea-8fcb-e058d959db70.png)

In this model, each LED is represented by a numbered white box that correspondes to a 2x2 px section of the scaled image in offset rows.  These 4 RBG values for each LED are then averaged and then output to a named csv file that will be the soruce for the real display.
