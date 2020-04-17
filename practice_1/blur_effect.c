#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define CHANNELS 3

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

// Error utility macro
#define ON_ERROR_EXIT(cond, message) \
do { \
    if((cond)) { \
        printf("Error in function: %s at line %d\n", __func__, __LINE__); \
        perror((message)); \
        exit(1); \
    } \
} while(0)

enum allocation_type{
	NO_ALLOCATION, SELF_ALLOCATED, STB_ALLOCATED
};

typedef struct{
	int width;
	int height;
	int kernel;
	int channels;
	int *rgb[CHANNELS];
	int *targetsRGB[CHANNELS];
	size_t size;
	uint8_t *data;
	enum allocation_type allocation_;
}Image;

Image img;
int THREADS;

int max(int num1, int num2){
    return (num1 > num2 ) ? num1 : num2;
}

int min(int num1, int num2) {
    return (num1 > num2 ) ? num2 : num1;
}

void imageFree(Image *img){
	if(img->allocation_ != NO_ALLOCATION && img->data != NULL){
		int i;
		stbi_image_free(img->data);
		img->data = NULL;
		img->width = 0;
		img->height = 0;
		img->size = 0;
		img->allocation_ = NO_ALLOCATION;

		for(i = 0; i < CHANNELS; i++){
			free(img->rgb[i]);
			free(img->targetsRGB[i]);
		}
	}
}

void imageLoad(Image *img, const char *fname, int kernel){
	unsigned char *p;
	int i = 0;
	
	img->data = stbi_load(fname, &img->width, &img->height, &img->channels, 0);
	ON_ERROR_EXIT(img->data == NULL, "Error in stbi_load");

	img->size = img->width * img->height * img->channels;
	img->allocation_ = STB_ALLOCATED;
	img->kernel = kernel;

	for(i = 0; i < CHANNELS; i++){
		img->rgb[i] = (int*)malloc(sizeof(int)*(img->size/CHANNELS));
		img->targetsRGB[i] = (int*)malloc(sizeof(int)*(img->size/CHANNELS));
	}

	for(p = img->data; p != img->data + img->size; p += img->channels, i++){
		*(img->rgb[0] + i) = (uint8_t) *p;
		*(img->rgb[1] + i) = (uint8_t) *(p + 1);
		*(img->rgb[2] + i) = (uint8_t) *(p + 2);
	}
}

void imageSave(const Image *img, const char *fname){
	int i,j;
	for(i = 0, j = 0; i < (img->size/3); i++, j+=3){
		img->data[j] = *(img->rgb[0] + i);
		img->data[j + 1] = *(img->rgb[1] + i);
		img->data[j + 2] = *(img->rgb[2] + i);
	}
	stbi_write_jpg(fname, img->width, img->height, img->channels, img->data, 100);
}

void boxesForGauss(double *sizes){
	int k = img.kernel,	n = CHANNELS,i;

	double wIdeal = sqrt((12 * k * k / n) + 1);
	double wl = floor(wIdeal);
	if(fmod(wl, 2.0) == 0.0)
		wl--;
	double wu = wl + 2;
	double mIdeal = (12*k*k - n*wl*wl - 4*n*wl - 3*n) / (-4*wl -4);
	double m = round(mIdeal);
	for(i = 0; i < n; i++)
		*(sizes + i) = (i < m ? wl : wu);
}

void boxBlurT(int *scl, int *tcl, int r, int threadId){
	int initIteration,endIteration,
		w = img.width,h = img.height,
		i, j, k, y;

	initIteration = (h / THREADS) * threadId;
	if(threadId == (THREADS - 1))
		endIteration = h;
	else
		endIteration = (h / THREADS)*(threadId + 1);
	
	for(i = initIteration; i < endIteration; i++)
		for(j = 0; j < w; j++){
			int val = 0;
			for(k = (i - r); k < (i + r + 1); k++){
				y = min(h - 1, max(0, k));
				val += *(scl + (y * w + j));
			}
			*(tcl + (i * w + j)) = val / (r+r+1);
		}
}

void boxBlurH(int *scl, int *tcl, int r, int threadId){
	int initIteration, endIteration,
		w = img.width, h = img.height,
		i, j, k, x;

	initIteration = (w / THREADS) * threadId;
	if(threadId == (THREADS - 1))
		endIteration = w;
	else
		endIteration = (w / THREADS)*(threadId + 1);
		
	for(i = 0; i < h; i++)
		for(j = initIteration; j < endIteration; j++){
			int val = 0;
			for(k = (j - r); k < (j + r + 1); k ++){
				x = min(w - 1, max(0, k));
				val += *(scl + (i * w + x));
			}
			*(tcl + (i * w + j)) = val/(r+r+1);
		}
}

void boxBlur(int *scl, int *tcl, int r,int threadId){
	int w = img.width, h = img.height, i;
	for(i = 0; i < (w*h); i++){
		int aux = *(scl + i);
		*(tcl + i) = aux;
	}
	boxBlurH(tcl, scl, r, threadId);
	boxBlurT(scl, tcl, r, threadId);
}

void gaussBlur_3(int *scl, int *tcl, int threadId){
	double *bxs = (double*)malloc(sizeof(double)*CHANNELS);
	boxesForGauss(bxs);
	boxBlur(scl, tcl, (int)((*(bxs)-1)/2), threadId);
	boxBlur(tcl, scl, (int)((*(bxs+1)-1)/2), threadId);
	boxBlur(scl, tcl, (int)((*(bxs+2)-1)/2), threadId);
}

void parallel(void *arg){
	const Image *orig = &img;
	int threadId = *(int *)arg,	i;

    ON_ERROR_EXIT(!(orig->allocation_ != NO_ALLOCATION && orig->channels >= 3), 
			"The input image must have at least 3 channels.");
	for(i = 0; i < CHANNELS; i++)
		gaussBlur_3(img.rgb[i], img.targetsRGB[i], threadId);
}

int main(int argc, char *argv[]){
	ON_ERROR_EXIT(argc < 5, "time ./blr imgInp.jpg imgOut.jpg kernel threads");
	THREADS = atoi(argv[4]);
	int kernel = atoi(argv[3]),
		threadId[THREADS],
		i;
	pthread_t thread[THREADS];

	imageLoad(&img, argv[1], kernel);
	for(i = 0; i < THREADS; i++){
		threadId[i] = i;
		pthread_create(&thread[i], NULL, (void *)parallel, &threadId[i]);
	}
	for(i = 0; i < THREADS; i++)
		pthread_join(thread[i], NULL);

	imageSave(&img, argv[2]);
	imageFree(&img);
	return 0;
}
// gcc -Wall blur_effect.c -o blur -lpthread -lm
