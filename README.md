# ledcsv
Converts a 24-bit Bitmap image (.bmp) to a 320 line .csv file with RBG values intended for a HERA display.
    You can convert to this image format using Microsoft Paint

You will need to first compile the program using the command: make ledcsv

Then you can run the program using the command: ./ledcsv [image] [csv]

    [image] needs to be a 24-bit Bitmap image (.bmp)
    [csv] needs to be a .csv file name that will be overwritten or created after it runs
    
A scaled image temp.bmp will also be created in the current directory

