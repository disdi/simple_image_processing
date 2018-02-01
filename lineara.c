#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
    // https://stackoverflow.com/a/23335691/1240328
#endif

#include <math.h>

void printUsage() {
    fprintf(stderr, "\n\n");
    fprintf(stderr, "                 ---  LinearA  ---\n\n");
    fprintf(stderr, "        is a tool to convert images to/from\n");
    fprintf(stderr, "             the pipeline-optimized\n");
    fprintf(stderr, "         image format, I call LEFPLRGBA.\n\n");

    fprintf(stderr, "     LE | Little-endian (like x86)\n");
    fprintf(stderr, "     FP | Floating-point (32-bit per channel)\n");
    fprintf(stderr, "   LRGB | Linear (0-1 without gamma) RGB\n");
    fprintf(stderr, "      A | with alpha channel (0 is transparent)\n\n");

    fprintf(stderr, "  Major features:\n");
    fprintf(stderr, "   + No compression. Because it's useless in IPC.\n");
    fprintf(stderr, "   + Linear! Blend colors, convert to CIEXYZ, etc.\n");
    fprintf(stderr, "   + Simple. A few lines of code needed to read it:\n");
    fprintf(stderr, "       - no magic number;\n");
    fprintf(stderr, "       - no data integrity control;\n");
    fprintf(stderr, "       - left-to-right top-to-bottom pixel order.\n\n");

    fprintf(stderr, "  Supported input formats:\n");
    fprintf(stderr, "     8-bit-per-channel sRGB(A)\n");
    fprintf(stderr, "       - JPEG\n");
    fprintf(stderr, "       - BMP\n");
    fprintf(stderr, "       - TGA\n");
    fprintf(stderr, "       - PNG\n\n");

    fprintf(stderr, "  Supported output formats:\n");
    fprintf(stderr, "     8-bit-per-channel sRGB(A)\n");
    fprintf(stderr, "       - PNG\n\n");

    fprintf(stderr, "  Usage:\n\n");

    fprintf(stderr, "    lineara image.png\n\n");
    fprintf(stderr, "      Prints LEFPLRGBA on the standard output.\n\n");

    fprintf(stderr, "    lineara [--back|-back|-b] out.png\n\n");
    fprintf(stderr, "      Reads LEFPLRGBA from the standard input and\n");
    fprintf(stderr, "      writes the result to a file. It detects format\n");
    fprintf(stderr, "      from the file name extension (PNG by default).\n\n");

    fprintf(stderr, "  For instance:\n\n");
    fprintf(stderr, "    lineara 1.bmp | appX | appY | lineara -b 2.jpg\n\n\n");

    fprintf(stderr, "                   License: MIT\n\n");

    #ifdef _WIN32
    fprintf(stderr, "    Based on \"stb\" libraries (MIT / public domain)\n\n\n");
    #else
    fprintf(stderr, "    Based on “stb” libraries (MIT / public domain)\n\n\n");
    #endif

}

unsigned char floatToByte(float n) {
    return (unsigned char) fmax(0.0, fmin(255.0, round(255.0 * n)));
}

float srgbToLinear(unsigned char cb) {
    float c = cb / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    } else {
        float a = 0.055;
        return pow(((c + a) / (1 + a)), 2.4);
    }
}

unsigned char linearToSrgb(float lin) {
    if (lin <= 0.0031308) {
        return floatToByte(lin * 12.92);
    } else {
        float a = 0.055;
        return floatToByte((1 + a) * pow(lin, 1.0/2.4) - a);
    }
}

int main(int argc, char *argv[]) {

    int32_t x;
    int32_t y;
    int channelCount;
    unsigned char * data;
    unsigned char * grey;

    #ifdef _WIN32
    _setmode(_fileno(stdout), O_BINARY);
    _setmode(_fileno(stdin), O_BINARY);
    #endif

    if (argc == 2) {

        int xt, yt, pos;
        data = stbi_load(argv[1], &xt, &yt, &channelCount, 0);
        fprintf(stderr, "Image %dx%d (%d channels).\n", xt, yt, channelCount);

        if (channelCount < STBI_rgb) {
            fprintf(stderr, "Grey and grey-alpha files are not supported yet.\n");
            exit(50);
        }

        x = xt;
        y = yt;

        fwrite(&x, sizeof(int32_t), 1, stdout);
        fwrite(&y, sizeof(int32_t), 1, stdout);

        long pxCount = x * y;
        float one = 1.0;

        float tempColor[3];

        for (pos = 0; pos < pxCount; pos++) {
            long numPos = pos * channelCount;

            tempColor[0] = srgbToLinear(data[numPos + 0]);
            tempColor[1] = srgbToLinear(data[numPos + 1]);
            tempColor[2] = srgbToLinear(data[numPos + 2]);
            fwrite(&tempColor, sizeof(float), 3, stdout);

            if (channelCount <= STBI_rgb) {

                fwrite(&one, sizeof(float), 1, stdout);

            } else {

                float temp = data[numPos + 3] / 255.0;
                 fwrite(&temp, sizeof(float), 1, stdout);

            }
        }

        stbi_image_free(data);

    } else if ((argc == 3) && (
            (strcmp("-b", argv[1]) == 0) ||
            (strcmp("-back", argv[1]) == 0) ||
            (strcmp("--back", argv[1]) == 0))) {

        fread(&x, sizeof(int32_t), 1, stdin);
        fread(&y, sizeof(int32_t), 1, stdin);

        fprintf(stderr, "STDIN image size: %dx%d.\n", x, y);

        channelCount = 4;
        data = (unsigned char *) malloc(x * y * channelCount * sizeof(unsigned char));
	grey = (unsigned char *) malloc(sizeof(unsigned char));
        float tempColor[4];
	int pos;

        long pxCount = x * y;
        for (pos = 0; pos < pxCount; pos++) {
            long numPos = pos * channelCount;

            fread(&tempColor, sizeof(float), 4, stdin);

	    *grey = linearToSrgb(tempColor[0]*0.2126) + linearToSrgb(tempColor[1]*0.7152) + linearToSrgb(tempColor[2]*0.0722);

            data[numPos + 0] = *grey;
            data[numPos + 1] = *grey;
            data[numPos + 2] = *grey;
            data[numPos + 3] = floatToByte(tempColor[3]);
        }
        stbi_write_png(argv[2], x, y, channelCount, (void *) data, 0);

    } else {
        printUsage();
        return 1;
    }


    return 0;
}
