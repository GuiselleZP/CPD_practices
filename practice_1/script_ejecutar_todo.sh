#! /bin/bash

gcc -Wall blur_effect.c -o blur -pthread -lm


echo "" > resultados.txt

#Mediciones del programa secuencial para kernel entre 3 y 15
echo "**********************************" >> resultados.txt
echo "----------------------------------" >> resultados.txt
echo "       Ejecucion secuencial       " >> resultados.txt
echo "----------------------------------" >> resultados.txt
echo "**********************************" >> resultados.txt

kernel=3
threads=1
echo "----------------------------------" >> resultados.txt
echo "         imagen 720p              " >> resultados.txt
echo "----------------------------------" >> resultados.txt
while [ $kernel -lt 16 ];
    do
    echo "Con un kernel de" $kernel >> resultados.txt
    { time ./blur lobo720.jpg lobo720blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
    echo "" >> resultados.txt
    kernel=$[$kernel+2]
done

kernel=3
threads=1
echo " "
echo "----------------------------------" >> resultados.txt
echo "          imagen 1080p            " >> resultados.txt
echo "----------------------------------" >> resultados.txt
while [ $kernel -lt 16 ];
    do
    echo "Con un kernel de" $kernel >> resultados.txt
    { time ./blur lobo1080.jpg lobo1080blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
    echo "" >> resultados.txt
    kernel=$[$kernel+2]
done

kernel=3
threads=1
echo " "
echo "----------------------------------" >> resultados.txt
echo "          imagen 4k               " >> resultados.txt
echo "----------------------------------" >> resultados.txt
while [ $kernel -lt 16 ];
    do
    echo "Con un kernel de" $kernel >> resultados.txt
    { time ./blur lobo4k.jpg lobo4kblur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
    echo "" >> resultados.txt
    kernel=$[$kernel+2]
done


##################################################################################################
##################################################################################################
########################################Paralelo##################################################
##################################################################################################
##################################################################################################


#Resultados del programa paralelizado para kernel entre 3 y 15
echo "" >> resultados.txt
echo "**********************************" >> resultados.txt
echo "----------------------------------" >> resultados.txt
echo "      Ejecicion en paralelo       " >> resultados.txt
echo "----------------------------------" >> resultados.txt
echo "**********************************" >> resultados.txt

kernel=3
threads=2
echo "----------------------------------" >> resultados.txt
echo "            imagen 720p           " >> resultados.txt
echo "----------------------------------" >> resultados.txt

while [ $kernel -lt 16 ];
    do
	while [ $threads -lt 17 ];
        do
        echo "Con un numero de hilos de" $threads >> resultados.txt
        echo "Con un kernel de" $kernel >> resultados.txt
        { time ./blur lobo720.jpg lobo720blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
	echo "" >> resultados.txt
	echo "" >> resultados.txt
	threads=$[$threads*2]
    done
    kernel=$[$kernel+2]
    threads=2
done

kernel=3
threads=2
echo "----------------------------------" >> resultados.txt
echo "          imagen 1080p            " >> resultados.txt
echo "----------------------------------" >> resultados.txt

while [ $kernel -lt 16 ];
    do
	while [ $threads -lt 17 ];
        do
        echo "Con un numero de hilos de" $threads >> resultados.txt
        echo "Con un kernel de" $kernel >> resultados.txt
        { time ./blur lobo1080.jpg lobo1080blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
	echo "" >> resultados.txt
	echo "" >> resultados.txt
	threads=$[$threads*2]
    done
    kernel=$[$kernel+2]
    threads=2
done

kernel=3
threads=2
echo "----------------------------------" >> resultados.txt
echo "             imagen 4k            " >> resultados.txt
echo "----------------------------------" >> resultados.txt

while [ $kernel -lt 16 ];
    do
	while [ $threads -lt 17 ];
        do
        echo "Con un numero de hilos de" $threads >> resultados.txt
        echo "Con un kernel de" $kernel >> resultados.txt
        { time ./blur lobo4k.jpg lobo4kblur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
	echo "" >> resultados.txt
	echo "" >> resultados.txt
	threads=$[$threads*2]
    done
    kernel=$[$kernel+2]
    threads=2
done
