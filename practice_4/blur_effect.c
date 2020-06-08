#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
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

void imageFree(unsigned char *data){
	if(data != NULL){
		int i;
		stbi_image_free(data);
		data = NULL;
		img.info.width = 0;
		img.info.height = 0;
		img.info.size = 0;

		for(i = 0; i < CHANNELS; i++){
			free(img.rgb[i]);
			free(img.targetsRGB[i]);
		}
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

void imageSave(const char *fname, unsigned char *data){
	int i,j;
	for(i = 0, j = 0; i < (img.info.size/3); i++, j+=3){
		data[j] = *(img.rgb[0] + i);
		data[j + 1] = *(img.rgb[1] + i);
		data[j + 2] = *(img.rgb[2] + i);
	}
	stbi_write_jpg(fname, img.info.width, img.info.height, img.info.channels, data, 100);
}

void boxesForGauss(double *sizes){
	int k = img.info.kernel,	n = CHANNELS,i;

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
	int initIteration,endIteration,
		w = img.info.width,h = img.info.height,
		i, j, k, y;

	initIteration = (h / PROCESSES) * processId;
	if(processId == (PROCESSES - 1))
		endIteration = h;
	else
		endIteration = (h / PROCESSES)*(processId + 1);
	
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

void boxBlurH(int *scl, int *tcl, int r, int processId){
	int initIteration, endIteration,
		w = img.info.width, h = img.info.height,
		i, j, k, x;

	initIteration = (w / PROCESSES) * processId;
	if(processId == (PROCESSES - 1))
		endIteration = w;
	else
		endIteration = (w / PROCESSES)*(processId + 1);
		
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

void boxBlur(int *scl, int *tcl, int r,int processId){
	int w = img.info.width, h = img.info.height, i;
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

void mallocRGB(){
	int i;
	for(i = 0; i < img.info.channels; i++){
		img.rgb[i] = (int*)malloc(sizeof(int)*(img.info.size/CHANNELS));
		img.targetsRGB[i] = (int*)malloc(sizeof(int)*(img.info.size/CHANNELS));
	}
}

void parallel(int processId){
	int i;

    ON_ERROR_EXIT(!(img.info.channels >= 3),
			"The input image must have at least 3 channels.");
	for(i = 0; i < CHANNELS; i++)
		gaussBlur_3(img.rgb[i], img.targetsRGB[i], processId);
}

int main(int argc, char *argv[]){
	ON_ERROR_EXIT(argc < 4, "time ./blr imgInp.jpg imgOut.jpg kernel");

	int kernel = atoi(argv[3]),
		initIteration,
		endIteration,
		arrayLength,
		root = 0,
		tasks,
		temp,
		iam,
		i,
		j,
		k;
	unsigned char *data;

	MPI_Init(&argc, &argv);
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &tasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &iam);

	PROCESSES = tasks;

	// Create MPI structure

	int blocksCount = 6;
	int blocksLength[blocksCount];

	MPI_Datatype types[blocksCount];
	MPI_Datatype MPI_INFO;

	for(i = 0; i < blocksCount; i++){
		blocksLength[i] = 1;
		types[i] = MPI_INT;
	}

	MPI_Aint offsets[blocksCount];

	offsets[0] = offsetof(ImageInformation, width);
	offsets[1] = offsetof(ImageInformation, height);
	offsets[2] = offsetof(ImageInformation, kernel);
	offsets[3] = offsetof(ImageInformation, channels);
	offsets[4] = offsetof(ImageInformation, size);
	offsets[5] = offsetof(ImageInformation, sizeChannel);
	
	MPI_Type_create_struct(blocksCount, blocksLength, offsets, types, &MPI_INFO);
	MPI_Type_commit(&MPI_INFO);

	// blur algorithm
		
	if(iam == 0)
		data = imageLoad(argv[1], kernel, &img.info);

	MPI_Bcast(&img.info, 1, MPI_INFO, root, MPI_COMM_WORLD);

	if(iam != 0)
		data = malloc(sizeof(int)*img.info.size);
	
	mallocRGB();
	
	if(iam == 0)
		createCopy(img.rgb, data);

	for(i = 0; i < img.info.channels; i++)
		MPI_Bcast(img.rgb[i], img.info.sizeChannel, MPI_INT, root, MPI_COMM_WORLD);


	temp = img.info.width;
	initIteration = (temp / tasks) * iam;
	if(iam == (tasks - 1))
		endIteration = temp;
	else
		endIteration = (temp / tasks)*(iam + 1);
	arrayLength = temp / tasks;
	

	/*
	printf("tasks: %d, iam: %d, temp: %d, length: %d, ini: %d, end:%d\n",
			tasks, iam, temp, arrayLength, initIteration, endIteration);

	printf("\n");
	for(i = 0; i < tasks; i++)
		printf("PRE- ID: %d, *(img.rgb[0] + %d) : %d\n",
				iam, (i*arrayLength) + 2,  *(img.rgb[0] + (i*arrayLength + 2)));
	printf("\n");
	*/

	parallel(iam);

	for(i = 0; i < img.info.channels; i++){
		int aux[arrayLength];
		for(k = 0, j = initIteration; j < endIteration; j++, k++)
			aux[k] = *(img.rgb[i] + j);
		MPI_Gather(aux, arrayLength, MPI_INT, img.rgb[i], arrayLength, MPI_INT, root, MPI_COMM_WORLD);
	}

	/*
	printf("\n");
	for(i = 0; i < tasks; i++)
		printf("POST - ID: %d, *(img.rgb[0] + %d) : %d\n",
				iam, (i*arrayLength) + 2,  *(img.rgb[0] + (i*arrayLength + 2)));
	printf("\n");
	*/

	if(iam == 0){
		imageSave(argv[2], data);
		imageFree(data);
	}
	MPI_Finalize();	
	return 0;
}
// mpicc -Wall blur_effect.c -o blur -lm
// mpirun -np 2 blur lobo720.jpg out.jpg 15
