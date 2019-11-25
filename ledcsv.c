// *******************************************************************************************************
// Takes a 24-bit BMP file and scales it to a 43x42 px temp file (temp.bmp) and then outputs a named csv
// file (2nd argument) with RGB values for 320 premapped LED lights for a HERA display.
// *******************************************************************************************************

#include <stdio.h>
#include <stdlib.h>

#include "bmp.h"

int getLEDIndex (int x, int y);

int main(int argc, char *argv[])
{
    // ensure proper usage
    if (argc != 3)
    {
        fprintf(stderr, "Usage: ./ledcsv <bmp image name (input)> <csv file (output)>\n");
        return 1;
    }

    // remember filenames
    char *infile = argv[1];
    char *tempfile = "temp.bmp";
    char *outfile = argv[2];

    // open input file
    FILE *inptr = fopen(infile, "r");
    if (inptr == NULL)
    {
        fprintf(stderr, "Could not open %s.\n", infile);
        return 2;
    }

    // open temp file to create scaled image
    FILE *tempptr = fopen(tempfile, "w");
    if (tempptr == NULL)
    {
        fclose(inptr);
        fprintf(stderr, "Could not create %s.\n", tempfile);
        return 3;
    }

    // open output file
    FILE *outptr = fopen(outfile, "w");
    if (outptr == NULL)
    {
        fclose(inptr);
        fclose(tempptr);
        fprintf(stderr, "Could not create %s.\n", outfile);
        return 4;
    }

    // read infile's BITMAPFILEHEADER
    BITMAPFILEHEADER bf;
    fread(&bf, sizeof(BITMAPFILEHEADER), 1, inptr);

    // read infile's BITMAPINFOHEADER
    BITMAPINFOHEADER bi;
    fread(&bi, sizeof(BITMAPINFOHEADER), 1, inptr);

    // ensure infile is (likely) a 24-bit uncompressed BMP 4.0
    if (bf.bfType != 0x4d42 || bf.bfOffBits != 54 || bi.biSize != 40 ||
        bi.biBitCount != 24 || bi.biCompression != 0)
    {
        fclose(outptr);
        fclose(inptr);
        fclose(tempptr);
        fprintf(stderr, "Unsupported input file format.  Needs to be 24-bit Bitmap file (.bmp, use Paint to convert)\n");
        return 5;
    }

    // edit temp file's headers
    BITMAPFILEHEADER obf = bf;
    BITMAPINFOHEADER obi = bi;

    // dimensions of scaled image are predetermined
    obi.biWidth = 43;
    obi.biHeight = 42;

    // determine padding for scanlines
    int padding = (4 - (bi.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;
    int oPadding = (4 - (obi.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    // determine how many pixels are being discarded at the end of the row
    int cropping = (bi.biWidth % obi.biWidth) * sizeof(RGBTRIPLE);

    obi.biSizeImage = ((sizeof(RGBTRIPLE) * obi.biWidth) + oPadding) * abs(obi.biHeight);
    obf.bfSize = obi.biSizeImage + sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);

    // write temp file's BITMAPFILEHEADER
    fwrite(&obf, sizeof(BITMAPFILEHEADER), 1, tempptr);

    // write temp file's BITMAPINFOHEADER
    fwrite(&obi, sizeof(BITMAPINFOHEADER), 1, tempptr);

    int xIndex = 0;
    long red[43] = {0};
    long blue[43] = {0};
    long green[43] = {0};

    // figure out how many rows and columns of pixels from infile will make up 1 pixel in scaled image
    long pxColumns = bi.biWidth / obi.biWidth;
    long pxRows = bi.biHeight / obi.biHeight;

    // iterate over infile's scanlines
    for (int i = 0; i < pxRows * obi.biHeight; i++)
    {
        // iterate over pixels in scanline row
        for (int j = 0; j < pxColumns * obi.biWidth; j++)
        {
            RGBTRIPLE triple;

            // read RGB triple from infile
            fread(&triple, sizeof(RGBTRIPLE), 1, inptr);

            // determine which pixel this will translate to for the temp scaled image
            xIndex = j / pxColumns;

            // sum the RBG values that will correspond to pixels for the output file
            red[xIndex] += triple.rgbtRed;
            green[xIndex] += triple.rgbtGreen;
            blue[xIndex] += triple.rgbtBlue;
        }

        // discard the excess pixels on this row
        fseek(inptr, cropping, SEEK_CUR);

        // skip over input file padding, if any
        fseek(inptr, padding, SEEK_CUR);

        // check if we have reached the next row of pixels for output file
        if (((i + 1) / pxRows) > (i / pxRows))
        {
            for (int x = 0; x < obi.biWidth; x++)
            {
                RGBTRIPLE triple;

                // average the RGB values gathered above and write row to temp scaled image
                triple.rgbtRed = red[x] / (pxColumns * pxRows);
                triple.rgbtGreen = green[x] / (pxColumns * pxRows);
                triple.rgbtBlue = blue[x] / (pxColumns * pxRows);

                fwrite(&triple, sizeof(RGBTRIPLE), 1, tempptr);
            }
            // add output padding
            for (int j = 0; j < oPadding; j++)
            {
                fputc(0x00, tempptr);
            }

            // clear out old data and start fresh for next row
            for (int x = 0; x < obi.biWidth; x++)
            {
                red[x] = 0;
                green[x] = 0;
                blue[x] = 0;
            }
        }
    }

    // close infile
    fclose(inptr);

    // close tempfile
    fclose(tempptr);

    // open scaled image for reading
    FILE *scaled = fopen(tempfile, "r");
    if (scaled == NULL)
    {
        fprintf(stderr, "Could not open scaled image");
        return 6;
    }

    // seek past scaled headers
    fseek(scaled, sizeof(BITMAPFILEHEADER), SEEK_CUR);
    fseek(scaled, sizeof(BITMAPINFOHEADER), SEEK_CUR);

    // set up structure for numbered LEDs
    RGBTRIPLE *led = malloc(320 * sizeof(RGBTRIPLE));
    long redSum[320];
    long greenSum[320];
    long blueSum[320];
    for (int n = 0; n < 320; n++)
    {
        redSum[n] = 0;
        greenSum[n] = 0;
        blueSum[n] = 0;
        led[n].rgbtRed = 0;
        led[n].rgbtGreen = 0;
        led[n].rgbtBlue = 0;
    }
    int ledNumber;

    // iterate over scaled file's scanlines
    for (int i = obi.biHeight - 1; i >= 0; i--)
    {
        // iterate over pixels in scanline
        for (int j = 0; j < obi.biWidth; j++)
        {
            RGBTRIPLE triple;

            // read RGB triple from infile
            fread(&triple, sizeof(RGBTRIPLE), 1, scaled);

            // only save info on valid LEDs from the scaled image
            ledNumber = getLEDIndex(j, i);
            if (ledNumber != -1)
            {
                // sum RBG values for averaging later
                redSum[ledNumber] += triple.rgbtRed;
                greenSum[ledNumber] += triple.rgbtGreen;
                blueSum[ledNumber] += triple.rgbtBlue;
            }
        }

        // skip over scaled file padding, if any
        fseek(scaled, oPadding, SEEK_CUR);
    }

    // create named csv output file with above RGB values
    for (int n = 0; n < 320; n++)
    {
        led[n].rgbtRed = (redSum[n] / 4);
        led[n].rgbtGreen = (greenSum[n] / 4);
        led[n].rgbtBlue = (blueSum[n] / 4);
        fprintf(outptr, "%i, %i, %i, %i", n, led[n].rgbtRed, led[n].rgbtGreen, led[n].rgbtBlue);
        if (n < 319)
        {
            fprintf(outptr, "\n");
        }
    }

    fclose(scaled);
    fclose(outptr);

    // success
    return 0;
}

// maps specific x,y coordinates of the pixels from scaled image to predetermined LED numbers for csv
int getLEDIndex (int x, int y)
{
    if (y == 0 || y == 1)
    {
        if (x == 9 || x == 10)
        {
            return 220;
        }
        else if (x == 11 || x == 12)
        {
            return 219;
        }
        else if (x == 13 || x == 14)
        {
            return 218;
        }
        else if (x == 15 || x == 16)
        {
            return 217;
        }
        else if (x == 17 || x == 18)
        {
            return 216;
        }
        else if (x == 19 || x == 20)
        {
            return 215;
        }
        else if (x == 21 || x == 22)
        {
            return 214;
        }
        else if (x == 23 || x == 24)
        {
            return 213;
        }
        else if (x == 25 || x == 26)
        {
            return 212;
        }
        else if (x == 27 || x == 28)
        {
            return 211;
        }
        else if (x == 29 || x == 30)
        {
            return 210;
        }
    }
    else if (y == 2 || y == 3)
    {
        if (x == 8 || x == 9)
        {
            return 221;
        }
        else if (x == 10 || x == 11)
        {
            return 222;
        }
        else if (x == 12 || x == 13)
        {
            return 223;
        }
        else if (x == 14 || x == 15)
        {
            return 224;
        }
        else if (x == 16 || x == 17)
        {
            return 225;
        }
        else if (x == 18 || x == 19)
        {
            return 226;
        }
        else if (x == 20 || x == 21)
        {
            return 227;
        }
        else if (x == 22 || x == 23)
        {
            return 228;
        }
        else if (x == 24 || x == 25)
        {
            return 229;
        }
        else if (x == 26 || x == 27)
        {
            return 230;
        }
        else if (x == 28 || x == 29)
        {
            return 231;
        }
        else if (x == 32 || x == 33)
        {
            return 209;
        }
    }
    else if (y == 4 || y == 5)
    {
        if (x == 7 || x == 8)
        {
            return 242;
        }
        else if (x == 9 || x == 10)
        {
            return 241;
        }
        else if (x == 11 || x == 12)
        {
            return 240;
        }
        else if (x == 13 || x == 14)
        {
            return 239;
        }
        else if (x == 15 || x == 16)
        {
            return 238;
        }
        else if (x == 17 || x == 18)
        {
            return 237;
        }
        else if (x == 19 || x == 20)
        {
            return 236;
        }
        else if (x == 21 || x == 22)
        {
            return 235;
        }
        else if (x == 23 || x == 24)
        {
            return 234;
        }
        else if (x == 25 || x == 26)
        {
            return 233;
        }
        else if (x == 27 || x == 28)
        {
            return 232;
        }
        else if (x == 31 || x == 32)
        {
            return 190;
        }
        else if (x == 33 || x == 34)
        {
            return 208;
        }
    }
    else if (y == 6 || y == 7)
    {
        if (x == 6 || x == 7)
        {
            return 243;
        }
        else if (x == 8 || x == 9)
        {
            return 244;
        }
        else if (x == 10 || x == 11)
        {
            return 245;
        }
        else if (x == 12 || x == 13)
        {
            return 246;
        }
        else if (x == 14 || x == 15)
        {
            return 247;
        }
        else if (x == 16 || x == 17)
        {
            return 248;
        }
        else if (x == 18 || x == 19)
        {
            return 249;
        }
        else if (x == 20 || x == 21)
        {
            return 250;
        }
        else if (x == 22 || x == 23)
        {
            return 251;
        }
        else if (x == 24 || x == 25)
        {
            return 252;
        }
        else if (x == 26 || x == 27)
        {
            return 253;
        }
        else if (x == 30 || x == 31)
        {
            return 189;
        }
        else if (x == 32 || x == 33)
        {
            return 191;
        }
        else if (x == 34 || x == 35)
        {
            return 207;
        }
    }
    else if (y == 8 || y == 9)
    {
        if (x == 5 || x == 6)
        {
            return 264;
        }
        else if (x == 7 || x == 8)
        {
            return 263;
        }
        else if (x == 9 || x == 10)
        {
            return 262;
        }
        else if (x == 11 || x == 12)
        {
            return 261;
        }
        else if (x == 13 || x == 14)
        {
            return 260;
        }
        else if (x == 15 || x == 16)
        {
            return 259;
        }
        else if (x == 17 || x == 18)
        {
            return 258;
        }
        else if (x == 19 || x == 20)
        {
            return 257;
        }
        else if (x == 21 || x == 22)
        {
            return 256;
        }
        else if (x == 23 || x == 24)
        {
            return 255;
        }
        else if (x == 25 || x == 26)
        {
            return 254;
        }
        else if (x == 29 || x == 30)
        {
            return 170;
        }
        else if (x == 31 || x == 32)
        {
            return 188;
        }
        else if (x == 33 || x == 34)
        {
            return 192;
        }
        else if (x == 35 || x == 36)
        {
            return 206;
        }
    }
    else if (y == 10 || y == 11)
    {
        if (x == 4 || x == 5)
        {
            return 265;
        }
        else if (x == 6 || x == 7)
        {
            return 266;
        }
        else if (x == 8 || x == 9)
        {
            return 267;
        }
        else if (x == 10 || x == 11)
        {
            return 268;
        }
        else if (x == 12 || x == 13)
        {
            return 269;
        }
        else if (x == 14 || x == 15)
        {
            return 270;
        }
        else if (x == 16 || x == 17)
        {
            return 271;
        }
        else if (x == 18 || x == 19)
        {
            return 272;
        }
        else if (x == 20 || x == 21)
        {
            return 273;
        }
        else if (x == 22 || x == 23)
        {
            return 274;
        }
        else if (x == 24 || x == 25)
        {
            return 275;
        }
        else if (x == 28 || x == 29)
        {
            return 169;
        }
        else if (x == 30 || x == 31)
        {
            return 171;
        }
        else if (x == 32 || x == 33)
        {
            return 187;
        }
        else if (x == 34 || x == 35)
        {
            return 193;
        }
        else if (x == 36 || x == 37)
        {
            return 205;
        }
    }
    else if (y == 12 || y == 13)
    {
        if (x == 3 || x == 4)
        {
            return 286;
        }
        else if (x == 5 || x == 6)
        {
            return 285;
        }
        else if (x == 7 || x == 8)
        {
            return 284;
        }
        else if (x == 9 || x == 10)
        {
            return 283;
        }
        else if (x == 11 || x == 12)
        {
            return 282;
        }
        else if (x == 13 || x == 14)
        {
            return 281;
        }
        else if (x == 15 || x == 16)
        {
            return 280;
        }
        else if (x == 17 || x == 18)
        {
            return 279;
        }
        else if (x == 19 || x == 20)
        {
            return 278;
        }
        else if (x == 21 || x == 22)
        {
            return 277;
        }
        else if (x == 23 || x == 24)
        {
            return 276;
        }
        else if (x == 27 || x == 28)
        {
            return 150;
        }
        else if (x == 29 || x == 30)
        {
            return 168;
        }
        else if (x == 31 || x == 32)
        {
            return 172;
        }
        else if (x == 33 || x == 34)
        {
            return 186;
        }
        else if (x == 35 || x == 36)
        {
            return 194;
        }
        else if (x == 37 || x == 38)
        {
            return 204;
        }
    }
    else if (y == 14 || y == 15)
    {
        if (x == 2 || x == 3)
        {
            return 287;
        }
        else if (x == 4 || x == 5)
        {
            return 288;
        }
        else if (x == 6 || x == 7)
        {
            return 289;
        }
        else if (x == 8 || x == 9)
        {
            return 290;
        }
        else if (x == 10 || x == 11)
        {
            return 291;
        }
        else if (x == 12 || x == 13)
        {
            return 292;
        }
        else if (x == 14 || x == 15)
        {
            return 293;
        }
        else if (x == 16 || x == 17)
        {
            return 294;
        }
        else if (x == 18 || x == 19)
        {
            return 295;
        }
        else if (x == 20 || x == 21)
        {
            return 296;
        }
        else if (x == 22 || x == 23)
        {
            return 297;
        }
        else if (x == 26 || x == 27)
        {
            return 149;
        }
        else if (x == 28 || x == 29)
        {
            return 151;
        }
        else if (x == 30 || x == 31)
        {
            return 167;
        }
        else if (x == 32 || x == 33)
        {
            return 173;
        }
        else if (x == 34 || x == 35)
        {
            return 185;
        }
        else if (x == 36 || x == 37)
        {
            return 195;
        }
        else if (x == 38 || x == 39)
        {
            return 203;
        }
    }
    else if (y == 16 || y == 17)
    {
        if (x == 1 || x == 2)
        {
            return 308;
        }
        else if (x == 3 || x == 4)
        {
            return 307;
        }
        else if (x == 5 || x == 6)
        {
            return 306;
        }
        else if (x == 7 || x == 8)
        {
            return 305;
        }
        else if (x == 9 || x == 10)
        {
            return 304;
        }
        else if (x == 11 || x == 12)
        {
            return 303;
        }
        else if (x == 13 || x == 14)
        {
            return 302;
        }
        else if (x == 15 || x == 16)
        {
            return 301;
        }
        else if (x == 17 || x == 18)
        {
            return 300;
        }
        else if (x == 19 || x == 20)
        {
            return 299;
        }
        else if (x == 21 || x == 22)
        {
            return 298;
        }
        else if (x == 25 || x == 26)
        {
            return 130;
        }
        else if (x == 27 || x == 28)
        {
            return 148;
        }
        else if (x == 29 || x == 30)
        {
            return 152;
        }
        else if (x == 31 || x == 32)
        {
            return 166;
        }
        else if (x == 33 || x == 34)
        {
            return 174;
        }
        else if (x == 35 || x == 36)
        {
            return 184;
        }
        else if (x == 37 || x == 38)
        {
            return 196;
        }
        else if (x == 39 || x == 40)
        {
            return 202;
        }
    }
    else if (y == 18 || y == 19)
    {
        if (x == 0 || x == 1)
        {
            return 309;
        }
        else if (x == 2 || x == 3)
        {
            return 310;
        }
        else if (x == 4 || x == 5)
        {
            return 311;
        }
        else if (x == 6 || x == 7)
        {
            return 312;
        }
        else if (x == 8 || x == 9)
        {
            return 313;
        }
        else if (x == 10 || x == 11)
        {
            return 314;
        }
        else if (x == 12 || x == 13)
        {
            return 315;
        }
        else if (x == 14 || x == 15)
        {
            return 316;
        }
        else if (x == 16 || x == 17)
        {
            return 317;
        }
        else if (x == 18 || x == 19)
        {
            return 318;
        }
        else if (x == 20 || x == 21)
        {
            return 319;
        }
        else if (x == 24 || x == 25)
        {
            return 129;
        }
        else if (x == 26 || x == 27)
        {
            return 131;
        }
        else if (x == 28 || x == 29)
        {
            return 147;
        }
        else if (x == 30 || x == 31)
        {
            return 153;
        }
        else if (x == 32 || x == 33)
        {
            return 165;
        }
        else if (x == 34 || x == 35)
        {
            return 175;
        }
        else if (x == 36 || x == 37)
        {
            return 183;
        }
        else if (x == 38 || x == 39)
        {
            return 197;
        }
        else if (x == 40 || x == 41)
        {
            return 201;
        }
    }
    else if (y == 20 || y == 21)
    {
        if (x == 23 || x == 24)
        {
            return 110;
        }
        else if (x == 25 || x == 26)
        {
            return 128;
        }
        else if (x == 27 || x == 28)
        {
            return 132;
        }
        else if (x == 29 || x == 30)
        {
            return 146;
        }
        else if (x == 31 || x == 32)
        {
            return 154;
        }
        else if (x == 33 || x == 34)
        {
            return 164;
        }
        else if (x == 35 || x == 36)
        {
            return 176;
        }
        else if (x == 37 || x == 38)
        {
            return 182;
        }
        else if (x == 39 || x == 40)
        {
            return 198;
        }
        else if (x == 41 || x == 42)
        {
            return 200;
        }
    }
    else if (y == 22 || y == 23)
    {
        if (x == 0 || x == 1)
        {
            return 99;
        }
        else if (x == 2 || x == 3)
        {
            return 100;
        }
        else if (x == 4 || x == 5)
        {
            return 101;
        }
        else if (x == 6 || x == 7)
        {
            return 102;
        }
        else if (x == 8 || x == 9)
        {
            return 103;
        }
        else if (x == 10 || x == 11)
        {
            return 104;
        }
        else if (x == 12 || x == 13)
        {
            return 105;
        }
        else if (x == 14 || x == 15)
        {
            return 106;
        }
        else if (x == 16 || x == 17)
        {
            return 107;
        }
        else if (x == 18 || x == 19)
        {
            return 108;
        }
        else if (x == 20 || x == 21)
        {
            return 109;
        }
        else if (x == 24 || x == 25)
        {
            return 111;
        }
        else if (x == 26 || x == 27)
        {
            return 127;
        }
        else if (x == 28 || x == 29)
        {
            return 133;
        }
        else if (x == 30 || x == 31)
        {
            return 145;
        }
        else if (x == 32 || x == 33)
        {
            return 155;
        }
        else if (x == 34 || x == 35)
        {
            return 163;
        }
        else if (x == 36 || x == 37)
        {
            return 177;
        }
        else if (x == 38 || x == 39)
        {
            return 181;
        }
        else if (x == 40 || x == 41)
        {
            return 199;
        }
    }
    else if (y == 24 || y == 25)
    {
        if (x == 1 || x == 2)
        {
            return 98;
        }
        else if (x == 3 || x == 4)
        {
            return 97;
        }
        else if (x == 5 || x == 6)
        {
            return 96;
        }
        else if (x == 7 || x == 8)
        {
            return 95;
        }
        else if (x == 9 || x == 10)
        {
            return 94;
        }
        else if (x == 11 || x == 12)
        {
            return 93;
        }
        else if (x == 13 || x == 14)
        {
            return 92;
        }
        else if (x == 15 || x == 16)
        {
            return 91;
        }
        else if (x == 17 || x == 18)
        {
            return 90;
        }
        else if (x == 19 || x == 20)
        {
            return 89;
        }
        else if (x == 21 || x == 22)
        {
            return 88;
        }
        else if (x == 25 || x == 26)
        {
            return 112;
        }
        else if (x == 27 || x == 28)
        {
            return 126;
        }
        else if (x == 29 || x == 30)
        {
            return 134;
        }
        else if (x == 31 || x == 32)
        {
            return 144;
        }
        else if (x == 33 || x == 34)
        {
            return 156;
        }
        else if (x == 35 || x == 36)
        {
            return 162;
        }
        else if (x == 37 || x == 38)
        {
            return 178;
        }
        else if (x == 39 || x == 40)
        {
            return 180;
        }
    }
    else if (y == 26 || y == 27)
    {
        if (x == 2 || x == 3)
        {
            return 77;
        }
        else if (x == 4 || x == 5)
        {
            return 78;
        }
        else if (x == 6 || x == 7)
        {
            return 79;
        }
        else if (x == 8 || x == 9)
        {
            return 80;
        }
        else if (x == 10 || x == 11)
        {
            return 81;
        }
        else if (x == 12 || x == 13)
        {
            return 82;
        }
        else if (x == 14 || x == 15)
        {
            return 83;
        }
        else if (x == 16 || x == 17)
        {
            return 84;
        }
        else if (x == 18 || x == 19)
        {
            return 85;
        }
        else if (x == 20 || x == 21)
        {
            return 86;
        }
        else if (x == 22 || x == 23)
        {
            return 87;
        }
        else if (x == 26 || x == 27)
        {
            return 113;
        }
        else if (x == 28 || x == 29)
        {
            return 125;
        }
        else if (x == 30 || x == 31)
        {
            return 135;
        }
        else if (x == 32 || x == 33)
        {
            return 143;
        }
        else if (x == 34 || x == 35)
        {
            return 157;
        }
        else if (x == 36 || x == 37)
        {
            return 161;
        }
        else if (x == 38 || x == 39)
        {
            return 179;
        }
    }
    else if (y == 28 || y == 29)
    {
        if (x == 3 || x == 4)
        {
            return 76;
        }
        else if (x == 5 || x == 6)
        {
            return 75;
        }
        else if (x == 7 || x == 8)
        {
            return 74;
        }
        else if (x == 9 || x == 10)
        {
            return 73;
        }
        else if (x == 11 || x == 12)
        {
            return 72;
        }
        else if (x == 13 || x == 14)
        {
            return 71;
        }
        else if (x == 15 || x == 16)
        {
            return 70;
        }
        else if (x == 17 || x == 18)
        {
            return 69;
        }
        else if (x == 19 || x == 20)
        {
            return 68;
        }
        else if (x == 21 || x == 22)
        {
            return 67;
        }
        else if (x == 23 || x == 24)
        {
            return 66;
        }
        else if (x == 27 || x == 28)
        {
            return 114;
        }
        else if (x == 29 || x == 30)
        {
            return 124;
        }
        else if (x == 31 || x == 32)
        {
            return 136;
        }
        else if (x == 33 || x == 34)
        {
            return 142;
        }
        else if (x == 35 || x == 36)
        {
            return 158;
        }
        else if (x == 37 || x == 38)
        {
            return 160;
        }
    }
    else if (y == 30 || y == 31)
    {
        if (x == 4 || x == 5)
        {
            return 55;
        }
        else if (x == 6 || x == 7)
        {
            return 56;
        }
        else if (x == 8 || x == 9)
        {
            return 57;
        }
        else if (x == 10 || x == 11)
        {
            return 58;
        }
        else if (x == 12 || x == 13)
        {
            return 59;
        }
        else if (x == 14 || x == 15)
        {
            return 60;
        }
        else if (x == 16 || x == 17)
        {
            return 61;
        }
        else if (x == 18 || x == 19)
        {
            return 62;
        }
        else if (x == 20 || x == 21)
        {
            return 63;
        }
        else if (x == 22 || x == 23)
        {
            return 64;
        }
        else if (x == 24 || x == 25)
        {
            return 65;
        }
        else if (x == 28 || x == 29)
        {
            return 115;
        }
        else if (x == 30 || x == 31)
        {
            return 123;
        }
        else if (x == 32 || x == 33)
        {
            return 137;
        }
        else if (x == 34 || x == 35)
        {
            return 141;
        }
        else if (x == 36 || x == 37)
        {
            return 159;
        }
    }
    else if (y == 32 || y == 33)
    {
        if (x == 5 || x == 6)
        {
            return 54;
        }
        else if (x == 7 || x == 8)
        {
            return 53;
        }
        else if (x == 9 || x == 10)
        {
            return 52;
        }
        else if (x == 11 || x == 12)
        {
            return 51;
        }
        else if (x == 13 || x == 14)
        {
            return 50;
        }
        else if (x == 15 || x == 16)
        {
            return 49;
        }
        else if (x == 17 || x == 18)
        {
            return 48;
        }
        else if (x == 19 || x == 20)
        {
            return 47;
        }
        else if (x == 21 || x == 22)
        {
            return 46;
        }
        else if (x == 23 || x == 24)
        {
            return 45;
        }
        else if (x == 25 || x == 26)
        {
            return 44;
        }
        else if (x == 29 || x == 30)
        {
            return 116;
        }
        else if (x == 31 || x == 32)
        {
            return 122;
        }
        else if (x == 33 || x == 34)
        {
            return 138;
        }
        else if (x == 35 || x == 36)
        {
            return 140;
        }
    }
    else if (y == 34 || y == 35)
    {
        if (x == 6 || x == 7)
        {
            return 33;
        }
        else if (x == 8 || x == 9)
        {
            return 34;
        }
        else if (x == 10 || x == 11)
        {
            return 35;
        }
        else if (x == 12 || x == 13)
        {
            return 36;
        }
        else if (x == 14 || x == 15)
        {
            return 37;
        }
        else if (x == 16 || x == 17)
        {
            return 38;
        }
        else if (x == 18 || x == 19)
        {
            return 39;
        }
        else if (x == 20 || x == 21)
        {
            return 40;
        }
        else if (x == 22 || x == 23)
        {
            return 41;
        }
        else if (x == 24 || x == 25)
        {
            return 42;
        }
        else if (x == 26 || x == 27)
        {
            return 43;
        }
        else if (x == 30 || x == 31)
        {
            return 117;
        }
        else if (x == 32 || x == 33)
        {
            return 121;
        }
        else if (x == 34 || x == 35)
        {
            return 139;
        }
    }
    else if (y == 36 || y == 37)
    {
        if (x == 7 || x == 8)
        {
            return 32;
        }
        else if (x == 9 || x == 10)
        {
            return 31;
        }
        else if (x == 11 || x == 12)
        {
            return 30;
        }
        else if (x == 13 || x == 14)
        {
            return 29;
        }
        else if (x == 15 || x == 16)
        {
            return 28;
        }
        else if (x == 17 || x == 18)
        {
            return 27;
        }
        else if (x == 19 || x == 20)
        {
            return 26;
        }
        else if (x == 21 || x == 22)
        {
            return 25;
        }
        else if (x == 23 || x == 24)
        {
            return 24;
        }
        else if (x == 25 || x == 26)
        {
            return 23;
        }
        else if (x == 27 || x == 28)
        {
            return 22;
        }
        else if (x == 31 || x == 32)
        {
            return 118;
        }
        else if (x == 33 || x == 34)
        {
            return 120;
        }
    }
    else if (y == 38 || y == 39)
    {
        if (x == 8 || x == 9)
        {
            return 11;
        }
        else if (x == 10 || x == 11)
        {
            return 12;
        }
        else if (x == 12 || x == 13)
        {
            return 13;
        }
        else if (x == 14 || x == 15)
        {
            return 14;
        }
        else if (x == 16 || x == 17)
        {
            return 15;
        }
        else if (x == 18 || x == 19)
        {
            return 16;
        }
        else if (x == 20 || x == 21)
        {
            return 17;
        }
        else if (x == 22 || x == 23)
        {
            return 18;
        }
        else if (x == 24 || x == 25)
        {
            return 19;
        }
        else if (x == 26 || x == 27)
        {
            return 20;
        }
        else if (x == 28 || x == 29)
        {
            return 21;
        }
        else if (x == 32 || x == 33)
        {
            return 119;
        }
    }
    else if (y == 40 || y == 41)
    {
        if (x == 9 || x == 10)
        {
            return 10;
        }
        else if (x == 11 || x == 12)
        {
            return 9;
        }
        else if (x == 13 || x == 14)
        {
            return 8;
        }
        else if (x == 15 || x == 16)
        {
            return 7;
        }
        else if (x == 17 || x == 18)
        {
            return 6;
        }
        else if (x == 19 || x == 20)
        {
            return 5;
        }
        else if (x == 21 || x == 22)
        {
            return 4;
        }
        else if (x == 23 || x == 24)
        {
            return 3;
        }
        else if (x == 25 || x == 26)
        {
            return 2;
        }
        else if (x == 27 || x == 28)
        {
            return 1;
        }
        else if (x == 29 || x == 30)
        {
            return 0;
        }
    }
    return -1;
}