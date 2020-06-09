#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <mpi.h>

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
	int sizeChannels;
	int *rgb[CHANNELS];
	int *targetsRGB[CHANNELS];
	size_t size;
	uint8_t *data;
	enum allocation_type allocation_;
}Image;

Image img;
int numprocs;

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
	img->sizeChannels = img->width * img->height;

	for(i = 0; i < CHANNELS; i++){
		img->rgb[i] = (int*)malloc(sizeof(int)*(img->size/CHANNELS));
		img->targetsRGB[i] = (int*)malloc(sizeof(int)*(img->size/CHANNELS));
	}

	i = 0;
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

void boxBlurT(int *scl, int *tcl, int r, int processId){
	int initIterationH, endIterationH, initIterationW, endIterationW,
		w = img.width,h = img.height,
		i, j, k, y;

	/*
	initIterationH = (h / numprocs) * processId;
	if(processId == (numprocs - 1))
		endIterationH = h;
	else
		endIterationH = (h / numprocs)*(processId + 1);

	initIterationW = (w / numprocs) * processId;
	if(processId == (numprocs - 1))
		endIterationW = w;
	else
		endIterationW = (w / numprocs)*(processId + 1);

    initIterationH = (h/numprocs) * processId;
    endIterationH = (h/numprocs) + 1;

	initIterationW = (w/numprocs) * processId;
    endIterationW = (w/numprocs) + 1;
	*/
	for(i = 0; i < h; i++)
		for(j = 0; j < w; j++){
			int val = 0;
			for(k = (i - r); k < (i + r + 1); k++){
				y = min(h - 1, max(0, k));
				val += *(scl + (y * w + j));
			}
			*(tcl + (i * w + j)) = val / (r+r+1);
		}
}

void boxBlurH(int *scl, int *tcl, int r, int processId){
	int initIterationH, endIterationH, initIterationW, endIterationW,
		w = img.width, h = img.height,
		i, j, k, x;

	/*
	initIterationH = (h / numprocs) * processId;
	if(processId == (numprocs - 1))
		endIterationH = h;
	else
		endIterationH = (h / numprocs)*(processId + 1);

	initIterationW = (w / numprocs) * processId;
	if(processId == (numprocs - 1))
		endIterationW = w;
	else
		endIterationW = (w / numprocs)*(processId + 1);

    initIterationH = (h/numprocs) * processId;
    endIterationH = (h/numprocs) + 1;

    initIterationW = (w/numprocs) * processId;
    endIterationW = (w/numprocs) + 1;
	*/

	for(i = 0; i < h; i++)
		for(j = 0; j < w; j++){
			int val = 0;
			for(k = (j - r); k < (j + r + 1); k ++){
				x = min(w - 1, max(0, k));
				val += *(scl + (i * w + x));
			}
			*(tcl + (i * w + j)) = val/(r+r+1);
		}
}

void boxBlur(int *scl, int *tcl, int r,int processId){
	int w = img.width, h = img.height, i;
	for(i = 0; i < (w*h); i++){
		int aux = *(scl + i);
		*(tcl + i) = aux;
	}
	boxBlurH(tcl, scl, r, processId);
	boxBlurT(scl, tcl, r, processId);
}

void gaussBlur_3(int *scl, int *tcl, int processId){
	double *bxs = (double*)malloc(sizeof(double)*CHANNELS);
	boxesForGauss(bxs);
	boxBlur(scl, tcl, (int)((*(bxs)-1)/2), processId);
	boxBlur(tcl, scl, (int)((*(bxs+1)-1)/2), processId);
	boxBlur(scl, tcl, (int)((*(bxs+2)-1)/2), processId);
}

void parallel(int processId){
	int i;
	for(i = 0; i < CHANNELS; i++)
		gaussBlur_3(img.rgb[i], img.targetsRGB[i], processId);
}

int main(int argc, char *argv[]){
	ON_ERROR_EXIT(argc < 4, "time ./blr imgInp.jpg imgOut.jpg kernel");
	int processId, arrayLength;
	
	MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);
	int kernel = atoi(argv[3]);
	//width = img.width;
	if(processId == 0){
		imageLoad(&img, argv[1], kernel);
		arrayLength = img.width / (numprocs);
	}

	MPI_Bcast(&arrayLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&img.width, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&img.height, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&img.kernel, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&img.sizeChannels, 1, MPI_INT, 0, MPI_COMM_WORLD);

	//printf("\nprocess %d \n width %d height %d sizeChannels %i \n", processId, img.width, img.height, img.sizeChannels);

	
	if(processId != 0){
		for(int i = 0; i < CHANNELS; i++){
			img.rgb[i] = (int *)malloc(sizeof(int) * (img.sizeChannels));
			img.targetsRGB[i] = (int *)malloc(sizeof(int) * (img.sizeChannels));
			//printf("\nprocess %d \n size %ld", processId, sizeof(img.rgb[i]));
		}
	}

	for(int i = 0; i < CHANNELS; i++){
		MPI_Bcast(img.rgb[i], img.sizeChannels, MPI_INT, 0, MPI_COMM_WORLD);
	}

	//arrayLength = img.width / numprocs;

	/*	
	printf("\nr process %d \n %d \n", processId, *(img.rgb[0] + (arrayLength + 2)));
	printf("\ng process %d \n %d \n", processId, *(img.rgb[1] + (arrayLength + 2)));
	printf("\nb process %d \n %d \n", processId, *(img.rgb[2] + (arrayLength + 2)));
	

	int temp = img.width;
	int endIteration;
	int initIteration = (temp / numprocs) * processId;
	if(processId == (numprocs - 1))
		endIteration = temp;
	else
		endIteration = (temp / numprocs)*(processId + 1);
*/
	parallel(processId);

	for(int i = 0; i < 3; i++){
		//int aux[arrayLength];
		//for(int k = 0; k < arrayLength; k++)
		//	aux[k] = *(img.rgb[i] + (arrayLength + k));
		MPI_Gather(img.rgb[i], arrayLength, MPI_INT, img.rgb[i], arrayLength, MPI_INT, 0, MPI_COMM_WORLD);
	}
	/*

	for(int i = 0; i < 3; i++){
		int aux[arrayLength];
		for(int k = 0, j = initIteration; j < endIteration; j++, k++)
			aux[k] = *(img.rgb[i] + j);
		MPI_Gather(aux, arrayLength, MPI_INT, img.rgb[i], arrayLength, MPI_INT, 0, MPI_COMM_WORLD);
	}*/

	//MPI_Gather((void *)&buff2send, 1, MPI_INT, buff2recv, 1, MPI_INT, root, MPI_COMM_WORLD);

	/*
	THREADS = atoi(argv[4]);
	int kernel = atoi(argv[3]),
		threadId[THREADS],
		i;
	pthread_t thread[THREADS];

	
	for(i = 0; i < THREADS; i++){
		threadId[i] = i;
		pthread_create(&thread[i], NULL, (void *)parallel, &threadId[i]);
	}
	for(i = 0; i < THREADS; i++)
		pthread_join(thread[i], NULL);
	*/
	if(processId == 0){
		imageSave(&img, argv[2]);
		imageFree(&img);
	}

	MPI_Finalize();
	return 0;
}
// mpicc -Wall blur_effect.c -o blur -lm
// mpirun -np 4 blur lobo720.jpg loboblur720.jpg 3