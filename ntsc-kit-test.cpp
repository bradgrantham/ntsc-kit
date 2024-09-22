#include <map>
#include <set>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstring>

extern "C" {

#include "ntsc-kit.h"
#include "ntsc-kit-platform.h"

};

unsigned int imageWidth;
unsigned int imageHeight;
float *imagePixels;

static void skipComments(FILE *fp, char ***comments, size_t *commentCount)
{
    int c;
    char line[512];

    while((c = fgetc(fp)) == '#') {
        fgets(line, sizeof(line) - 1, fp);
	line[strlen(line) - 1] = '\0';
	if(comments != NULL) {
	    *comments =
	        (char**)realloc(*comments, sizeof(char *) * (*commentCount + 1));
	    (*comments)[*commentCount] = strdup(line);
	}
	if(commentCount != NULL) {
	    (*commentCount) ++;
	}
    }
    ungetc(c, fp);
}


int pnmRead(FILE *file, unsigned int *w, unsigned int *h, float **pixels,
    char ***comments, size_t *commentCount)
{
    unsigned char	dummyByte;
    int			i;
    float		max;
    char		token;
    int			width, height;
    float		*rgbaPixels;

    if(commentCount != NULL)
	*commentCount = 0;
    if(comments != NULL)
	*comments = NULL;

    fscanf(file, " ");

    skipComments(file, comments, commentCount);

    if(fscanf(file, "P%c ", &token) != 1) {
         fprintf(stderr, "pnmRead: Had trouble reading PNM tag\n");
	 return(0);
    }

    skipComments(file, comments, commentCount);

    if(fscanf(file, "%d ", &width) != 1) {
         fprintf(stderr, "pnmRead: Had trouble reading PNM width\n");
	 return(0);
    }

    skipComments(file, comments, commentCount);

    if(fscanf(file, "%d ", &height) != 1) {
         fprintf(stderr, "pnmRead: Had trouble reading PNM height\n");
	 return(0);
    }

    skipComments(file, comments, commentCount);

    if(token != '1' && token != '4')
        if(fscanf(file, "%f", &max) != 1) {
             fprintf(stderr, "pnmRead: Had trouble reading PNM max value\n");
	     return(0);
        }

    rgbaPixels = (float*)malloc(width * height * 4 * sizeof(float));
    if(rgbaPixels == NULL) {
         fprintf(stderr, "pnmRead: Couldn't allocate %lu bytes\n",
	     width * height * 4 * sizeof(float));
         fprintf(stderr, "pnmRead: (For a %d by %d image)\n", width,
	     height);
	 return(0);
    }

    if(token != '4')
	skipComments(file, comments, commentCount);

    if(token != '4')
    fread(&dummyByte, 1, 1, file);	/* chuck white space */

    if(token == '1') {
	for(i = 0; i < width * height; i++) {
	    int pixel;
	    fscanf(file, "%d", &pixel);
	    pixel = 1 - pixel;
	    rgbaPixels[i * 4 + 0] = pixel;
	    rgbaPixels[i * 4 + 1] = pixel;
	    rgbaPixels[i * 4 + 2] = pixel;
	    rgbaPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '2') {
	for(i = 0; i < width * height; i++) {
	    int pixel;
	    fscanf(file, "%d", &pixel);
	    rgbaPixels[i * 4 + 0] = pixel / max;
	    rgbaPixels[i * 4 + 1] = pixel / max;
	    rgbaPixels[i * 4 + 2] = pixel / max;
	    rgbaPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '3') {
	for(i = 0; i < width * height; i++) {
	    int r, g, b;
	    fscanf(file, "%d %d %d", &r, &g, &b);
	    rgbaPixels[i * 4 + 0] = r / max;
	    rgbaPixels[i * 4 + 1] = g / max;
	    rgbaPixels[i * 4 + 2] = b / max;
	    rgbaPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '4') {
        int bitnum = 0;

	for(i = 0; i < width * height; i++) {
	    unsigned char pixel;
	    unsigned char value = 0;

	    if(bitnum == 0) {
	        fread(&value, 1, 1, file);
            }

	    pixel = (1 - ((value >> (7 - bitnum)) & 1));
	    rgbaPixels[i * 4 + 0] = pixel;
	    rgbaPixels[i * 4 + 1] = pixel;
	    rgbaPixels[i * 4 + 2] = pixel;
	    rgbaPixels[i * 4 + 3] = 1.0;

	    if(++bitnum == 8 || ((i + 1) % width) == 0)
	        bitnum = 0;
	}
    } else if(token == '5') {
	for(i = 0; i < width * height; i++) {
	    unsigned char pixel;
	    fread(&pixel, 1, 1, file);
	    rgbaPixels[i * 4 + 0] = pixel / max;
	    rgbaPixels[i * 4 + 1] = pixel / max;
	    rgbaPixels[i * 4 + 2] = pixel / max;
	    rgbaPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '6') {
	for(i = 0; i < width * height; i++) {
	    unsigned char rgb[3];
	    fread(rgb, 3, 1, file);
	    rgbaPixels[i * 4 + 0] = rgb[0] / max;
	    rgbaPixels[i * 4 + 1] = rgb[1] / max;
	    rgbaPixels[i * 4 + 2] = rgb[2] / max;
	    rgbaPixels[i * 4 + 3] = 1.0;
	}
    }
    *w = width;
    *h = height;
    *pixels = rgbaPixels;
    return(1);
}

uint8_t syncBlackValue;
uint8_t blackValue;
uint8_t whiteValue;

int init_video([[maybe_unused]]void* private_data, [[maybe_unused]]uint8_t blackValue_, [[maybe_unused]]uint8_t whiteValue_)
{
    blackValue = blackValue_;
    syncBlackValue = blackValue_;
    whiteValue = whiteValue_;
    return 1;
}

unsigned char YIQDegreesToDAC(float y, float i, float q, int degrees)
{
    float sine, cosine;
    if(degrees == 0) {
        sine = 0.544638f;
        cosine = 0.838670f;
    } else if(degrees == 90) {
        sine = 0.838670f;
        cosine = -0.544638f;
    } else if(degrees == 180) {
        sine = -0.544638f;
        cosine = -0.838670f;
    } else if(degrees == 270) {
        sine = -0.838670f;
        cosine = 0.544638f;
    } else {
        sine = 0;
        cosine = 0;
    }
    float signal = y + q * sine + i * cosine;

    return syncBlackValue + signal * (whiteValue - syncBlackValue);
}

// This is transcribed from the NTSC spec, double-checked.
void RGBToYIQ(float r, float g, float b, float *y, float *i, float *q)
{
    *y = .30f * r + .59f * g + .11f * b;
    *i = -.27f * (b - *y) + .74f * (r - *y);
    *q = .41f * (b - *y) + .48f * (r - *y);
}

NTSCLineConfig mode;
int interlaced = 0;
bool scope_bits = false;

// line 17 *thru* 502
void fill_line([[maybe_unused]]int frameIndex, int lineWithinField, int lineNumber, size_t maxSamples, uint8_t* lineBuffer)
{
    int lines = interlaced ? 480 : 240;

    int line = lineNumber;
    int imageY = line * imageHeight / lines;
    if(imageY < 0 || imageY >= imageHeight)
    {
        memset(lineBuffer, (blackValue + whiteValue) / 2, maxSamples);
        return;
    }

    switch(mode) 
    {
        case NTSC_LINE_SAMPLES_910:
            {
                int sample_phase_offset = ((frameIndex + lineWithinField) % 2 == 0) ? 0 : 2;
                for(int sample = 0; sample < maxSamples; sample++)
                {
                    int imageX = sample * imageWidth / maxSamples;
                    float *color = imagePixels + 4 * (imageX + imageY * imageWidth);
                    float y, i, q;
                    RGBToYIQ(color[0], color[1], color[2], &y, &i, &q);
                    uint8_t value = YIQDegreesToDAC(y, i, q, 90 * ((sample + sample_phase_offset) % 4));
                    lineBuffer[sample] = value;
                }
            }
            break;
        case NTSC_LINE_SAMPLES_912:
            {
                for(int sample = 0; sample < maxSamples; sample++)
                {
                    int imageX = sample * imageWidth / maxSamples;
                    float *color = imagePixels + 4 * (imageX + imageY * imageWidth);
                    float y, i, q;
                    RGBToYIQ(color[0], color[1], color[2], &y, &i, &q);
                    uint8_t value = YIQDegreesToDAC(y, i, q, 90 * (sample % 4));
                    lineBuffer[sample] = value;
                }
            }
            break;
        case NTSC_LINE_SAMPLES_1368:
            {
                for(int sample = 0; sample < maxSamples; sample++)
                {
                    int imageX = sample * imageWidth / maxSamples;
                    float *color = imagePixels + 4 * (imageX + imageY * imageWidth);
                    float y, i, q;
                    RGBToYIQ(color[0], color[1], color[2], &y, &i, &q);
                    uint8_t value = YIQDegreesToDAC(y, i, q, 60 * (sample % 6));
                    lineBuffer[sample] = value;
                }
            }
            break;
    }
    if(false)
    {
        static FILE *fp = nullptr;
        if(fp == nullptr)
        {
            fp = fopen("hmm.ppm", "wb");
            fprintf(fp, "P5 %zd %d 255\n", maxSamples, lines);
        }
        fwrite(lineBuffer, maxSamples, 1, fp);
    }
}

int needs_colorburst()
{
    return 1;
}

extern "C" {

// These correspond to the Rosa 8-bit DAC output

#define DAC_VALUE_LIMIT 0xFF

#define MAX_DAC_VOLTAGE 1.32f

uint8_t PlatformVoltageToDACValue(float voltage)
{
    if(voltage < 0.0f) {
        return 0x0;
    }
    uint32_t value = (uint32_t)(voltage / MAX_DAC_VOLTAGE * 255);
    if(value >= DAC_VALUE_LIMIT) {
        return DAC_VALUE_LIMIT;
    }
    return value;
}

int PlatformGetNTSCLineNumber()
{
    return 0;
}

void PlatformDisableNTSCScanout()
{
}

void PlatformEnableNTSCScanout([[maybe_unused]] NTSCLineConfig line_config, [[maybe_unused]] bool interlaced)
{
}

};

int main(int argc, char **argv)
{
    uint8_t buffer[1368 * NTSC_FRAME_LINES];
    int samples = 912;

    if(argc < 2) {
        fprintf(stderr, "usage: %s inputppm\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* progName = argv[0];
    argc--;
    argv++;

    while(argc > 2)
    {
        if(strcmp(argv[0], "--scope") == 0)
        {
            scope_bits = true;
            argc--;
            argv++;
        }
        else if(strcmp(argv[0], "-i") == 0)
        {
            interlaced = 1;
            argc--;
            argv++;
        }
        else
        {
            fprintf(stderr, "usage: %s [-i] [--scope] [numsamples] image.ppm\n", progName);
            exit(1);
        }
    }

    if(argc == 2)
    {
        samples = atoi(argv[0]);
        if(std::set<int>({910, 912, 1368}).count(samples) == 0)
        {
            fprintf(stderr, "unknown sample count %d\n", samples);
            exit(1);
        }
        argc--;
        argv++;
    }


    FILE *inputImage = fopen(argv[0], "rb");
    if(inputImage == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int success = pnmRead(inputImage, &imageWidth, &imageHeight, &imagePixels, NULL, NULL);
    fclose(inputImage);
    if(!success) {
        fprintf(stderr, "couldn't read PPM from %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int lines = interlaced ? 525 : 262;

    if(false)
    {
        FILE *fpp = fopen("blarg.ppm", "wb");
        fprintf(fpp, "P6 %d %d 255\n", imageWidth, imageHeight);
        for(int i = 0; i < imageHeight; i++)
        {
            for(int j = 0; j < imageWidth; j++)
            {
                uint8_t pixel[3];
                pixel[0] = imagePixels[4 * (j + i * imageWidth) + 0] * 255;
                pixel[1] = imagePixels[4 * (j + i * imageWidth) + 1] * 255;
                pixel[2] = imagePixels[4 * (j + i * imageWidth) + 2] * 255;
                fwrite(pixel, 3, 1, fpp);
            }
        }
        fclose(fpp);
    }

    NTSCInitialize();

    std::map<int, NTSCLineConfig> samples_to_line_enum = {
        {910, NTSC_LINE_SAMPLES_910},
        {912, NTSC_LINE_SAMPLES_912},
        {1368, NTSC_LINE_SAMPLES_1368},
    };
    mode = samples_to_line_enum.at(samples);

    NTSCSetMode(interlaced, mode, nullptr, init_video, nullptr, fill_line, needs_colorburst);

    for(int line = 0; line < NTSC_FRAME_LINES; line++) {
        NTSCFillLineBuffer(0, line, buffer + line * samples);
        if(scope_bits)
        {
            for(int sample = 0; sample < samples; sample++)
            {
                uint8_t *samplep = buffer + line * samples + sample;
                if(sample < 100)
                {
                    *samplep |= 0x01;
                }
                else
                {
                    *samplep &= ~0x01;
                }

                if(line == 0)
                {
                    *samplep |= 0x02;
                }
                else
                {
                    *samplep &= ~0x02;
                }

            }
        }

    }

    if(false) {
        FILE *fp = fopen("out.ppm", "wb");
        // Write 2X height and write out each line twice to aid visualization
        fprintf(fp, "P5 %d %d 255\n", samples, lines * 2);
        for(int line = 0; line < lines; line++)
        {
            fwrite(buffer + line * samples, samples, 1, fp);  
            fwrite(buffer + line * samples, samples, 1, fp);  
        }
        fclose(fp);
    }

    printf("#include <stdint.h>\n\n");
    printf("const uint8_t image_samples = %d\n;", samples);
    printf("const uint8_t image_lines = %d\n;", lines);
    printf("const uint8_t image[] = {");
    for(size_t i = 0; i < samples * lines; i++)
    {
        if(i % 12 == 0)
        {
            printf("\n");
        }
        printf("0x%02X, ", buffer[i]);
    }
    printf("};\n");
}
