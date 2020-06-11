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

typedef struct{
	int width;
	int height;
	int kernel;
	int channels;
	int size;
	int sizeChannel;
}ImageInformation;

typedef struct{
	int *rgb[CHANNELS];
	int *targetsRGB[CHANNELS];
	ImageInformation info;
}Image;

Image img;
int PROCESSES;

int max(int num1, int num2){
    return (num1 > num2 ) ? num1 : num2;
}

int min(int num1, int num2) {
    return (num1 > num2 ) ? num2 : num1;
}

void split_image(unsigned char *data, int **split, int threadId){
	int init, end,channels,
		i, j, k;
	channels = img.info.channels;

	init = img.info.sizeChannel * threadId;
	end = img.info.sizeChannel * (threadId + 1);

	for(i = (channels * init), j = 0; i < (channels * end); i += channels, j++)
		for(k = 0; k < channels; k++)
			*(split[k] + j) = data[i + k];
}

void mallocRGB(int iam){
	int i, auxSize;
	auxSize = img.info.sizeChannel;

	for(i = 0; i < img.info.channels; i++){
		img.rgb[i] = (int*)malloc(sizeof(int)*auxSize);
		img.targetsRGB[i] = (int*)malloc(sizeof(int)*auxSize);
	}
}

void freeRGB(int **rgb){
	int i;
	for(i = 0; i < CHANNELS; i++)
		free(rgb[i]);
}

void imageFree(unsigned char *data){
	if(data != NULL){
		stbi_image_free(data);
		data = NULL;
		img.info.width = 0;
		img.info.height = 0;
		img.info.size = 0;
		img.info.sizeChannel = 0;
	}
}

void createCopy(int **rgb, unsigned char *data){
	unsigned char *p;
	int i = 0;
	for(i = 0, p = data; p != data + img.info.size; p += img.info.channels, i++){
		*(rgb[0] + i) = (uint8_t) *p;
		*(rgb[1] + i) = (uint8_t) *(p + 1);
		*(rgb[2] + i) = (uint8_t) *(p + 2);
	}
}

unsigned char *imageLoad(const char *fname, int kernel, ImageInformation *info){
	unsigned char *data;

	data = stbi_load(fname, &info->width, &info->height, &info->channels, 0);
	ON_ERROR_EXIT(data == NULL, "Error in stbi_load");

	info->size = info->width * info->height * info->channels;
	info->sizeChannel = info->width * info->height;
	info->kernel = kernel;

	return data;
}

void imageSave(const char *fname, unsigned char *data, int **rgb){
	int i,j;
	for(i = 0, j = 0; i < (img.info.sizeChannel * PROCESSES); i++, j+=3){
		data[j] = *(rgb[0] + i);
		data[j + 1] = *(rgb[1] + i);
		data[j + 2] = *(rgb[2] + i);
	}
	stbi_write_jpg(fname, img.info.width, img.info.height, img.info.channels, data, 100);
}

void boxesForGauss(double *sizes){
	int k, n,i;

	k = img.info.kernel;
	n = img.info.channels;

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

void boxBlurT(int *scl, int *tcl, int r){
	int w ,h, i, j, k, y;
	w = img.info.width;
	h = img.info.height;

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

void boxBlurH(int *scl, int *tcl, int r){
	int w , h, i, j, k, x;
	w = img.info.width;
	h = img.info.height;

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

void boxBlur(int *scl, int *tcl, int r){
	int w = img.info.width, h = img.info.height, i;
	for(i = 0; i < (w*h); i++){
		int aux = *(scl + i);
		*(tcl + i) = aux;
	}
	boxBlurH(tcl, scl, r);
	boxBlurT(scl, tcl, r);
}

void gaussBlur_3(int *scl, int *tcl){
	double *bxs = (double*)malloc(sizeof(double)*img.info.channels);
	boxesForGauss(bxs);
	boxBlur(scl, tcl, (int)((*(bxs)-1)/2));
	boxBlur(tcl, scl, (int)((*(bxs+1)-1)/2));
	boxBlur(scl, tcl, (int)((*(bxs+2)-1)/2));
}

void parallel(){
	int i;
    ON_ERROR_EXIT(img.info.channels < 3,"The input image must have at least 3 channels.");
	for(i = 0; i < CHANNELS; i++)
		gaussBlur_3(img.rgb[i], img.targetsRGB[i]);
}

int main(int argc, char *argv[]){
	ON_ERROR_EXIT(argc < 4, "time ./blr imgInp.jpg imgOut.jpg kernel");

	int kernel = atoi(argv[3]),
		root = 0,
		tasks,
		iam,
		i, j;
	unsigned char *data;

	MPI_Init(&argc, &argv);
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &tasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &iam);

	PROCESSES = tasks;

	int *globalRGB[CHANNELS];

	if(iam == 0){
		data = imageLoad(argv[1], kernel, &img.info);
		img.info.height /= PROCESSES;
		img.info.sizeChannel = img.info.height * img.info.width;
		img.info.size = img.info.sizeChannel * img.info.channels;

		for(i = 0; i < 3; i++)
			globalRGB[i] = (int*)malloc(sizeof(int)*img.info.sizeChannel*PROCESSES);
	}

	MPI_Bcast(&img.info.width, 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&img.info.height, 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&img.info.kernel, 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&img.info.channels, 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&img.info.size, 1, MPI_INT, root, MPI_COMM_WORLD);
	MPI_Bcast(&img.info.sizeChannel, 1, MPI_INT, root, MPI_COMM_WORLD);

	mallocRGB(iam);

	int MSG_LENGHT = img.info.sizeChannel;

	if(iam == 0){
		for(i = 1; i < tasks; i++){
			split_image(data, img.rgb, i);

			for(j = 0; j < img.info.channels; j ++)
				MPI_Send(img.rgb[j], MSG_LENGHT, MPI_INT, i, 1, MPI_COMM_WORLD);
		}
		split_image(data, img.rgb, iam);
	}else{
		for(i = 0; i < img.info.channels; i++)
			MPI_Recv(img.rgb[i], MSG_LENGHT, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
	}

	parallel();
	
	MSG_LENGHT = img.info.width * img.info.height;
	int arr[MSG_LENGHT];
	for(i = 0; i < img.info.channels; i++){
		for(j = 0; j < MSG_LENGHT; j++)
			arr[j] = *(img.rgb[i] + j);
		MPI_Gather(arr, MSG_LENGHT, MPI_INT, globalRGB[i], MSG_LENGHT, MPI_INT, 0, MPI_COMM_WORLD);
	}

	if(iam == 0){
		img.info.height *= PROCESSES;
		imageSave(argv[2], data, globalRGB);
		imageFree(data);
		freeRGB(globalRGB);
	}
	freeRGB(img.rgb);
	freeRGB(img.targetsRGB);
	MPI_Finalize();
	return 0;
}
// mpicc -Wall blur_effect.c -o blur -lm
// mpirun -np 2 blur lobo720.jpg out.jpg 15
