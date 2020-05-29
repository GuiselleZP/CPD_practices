#! /bin/bash

gcc -Wall -fopenmp blur_effect.c -o blur -lm

TIMEFORMAT=%E

echo "" > resultados.txt

#Mediciones del programa secuencial para kernel entre 3 y 15
echo "----------------------------------" >> resultados.txt
echo "       Ejecucion secuencial       " >> resultados.txt
echo "----------------------------------" >> resultados.txt

kernel=3
threads=1
iteracion=1
echo "----------------------------------" >> resultados.txt
echo "         imagen 720p              " >> resultados.txt
echo "----------------------------------" >> resultados.txt
while [ $kernel -lt 16 ];
    do
    echo "k$kernel: [" >> resultados.txt
    while [ $iteracion -lt 11 ];
    	do
	{ time ./blur lobo720.jpg lobo720blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
         iteracion=$[$iteracion+1]
    done
    echo "]," >> resultados.txt
    echo "" >> resultados.txt    
    kernel=$[$kernel+2]
    iteracion=1
done

kernel=3
threads=1
iteracion=1
echo " "
echo "----------------------------------" >> resultados.txt
echo "          imagen 1080p            " >> resultados.txt
echo "----------------------------------" >> resultados.txt
while [ $kernel -lt 16 ];
    do
    echo "k$kernel: [" >> resultados.txt
    while [ $iteracion -lt 11 ];
    	do
	{ time ./blur lobo1080.jpg lobo1080blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
         iteracion=$[$iteracion+1]
    done
    echo "]," >> resultados.txt
    echo "" >> resultados.txt    
    kernel=$[$kernel+2]
    iteracion=1
done


kernel=3
threads=1
iteracion=1
echo " "
echo "----------------------------------" >> resultados.txt
echo "          imagen 4k               " >> resultados.txt
echo "----------------------------------" >> resultados.txt
while [ $kernel -lt 16 ];
    do
    echo "k$kernel: [" >> resultados.txt
    while [ $iteracion -lt 11 ];
    	do
	{ time ./blur lobo4k.jpg lobo4kblur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
         iteracion=$[$iteracion+1]
    done
    echo "]," >> resultados.txt
    echo "" >> resultados.txt    
    kernel=$[$kernel+2]
    iteracion=1
done

########################################Paralelo##################################################


#Resultados del programa paralelizado para kernel entre 3 y 15
echo "" >> resultados.txt
echo "----------------------------------" >> resultados.txt
echo "      Ejecicion en paralelo       " >> resultados.txt
echo "----------------------------------" >> resultados.txt

kernel=3
threads=2
iteracion=1
echo "----------------------------------" >> resultados.txt
echo "            imagen 720p           " >> resultados.txt
echo "----------------------------------" >> resultados.txt

while [ $kernel -lt 16 ];
    do
    while [ $threads -lt 17 ];
        do
        echo "k$kernel _h$threads: [" >> resultados.txt
        while [ $iteracion -lt 11 ];
    	    do
	    { time ./blur lobo720.jpg lobo720blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
            iteracion=$[$iteracion+1]
        done
	echo "]," >> resultados.txt
	echo "" >> resultados.txt
	threads=$[$threads*2]
	iteracion=1
    done
    kernel=$[$kernel+2]
    threads=2
    iteracion=1
done

kernel=3
threads=2
iteracion=1
echo "----------------------------------" >> resultados.txt
echo "          imagen 1080p            " >> resultados.txt
echo "----------------------------------" >> resultados.txt

while [ $kernel -lt 16 ];
    do
    while [ $threads -lt 17 ];
        do
        echo "k$kernel _h$threads: [" >> resultados.txt
        while [ $iteracion -lt 11 ];
    	    do
	    { time ./blur lobo1080.jpg lobo1080blur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
            iteracion=$[$iteracion+1]
        done
	echo "]," >> resultados.txt
	echo "" >> resultados.txt
	threads=$[$threads*2]
	iteracion=1
    done
    kernel=$[$kernel+2]
    threads=2
    iteracion=1
done

kernel=3
threads=2
iteracion=1
echo "----------------------------------" >> resultados.txt
echo "             imagen 4k            " >> resultados.txt
echo "----------------------------------" >> resultados.txt

while [ $kernel -lt 16 ];
    do
    while [ $threads -lt 17 ];
        do
        echo "k$kernel _h$threads: [" >> resultados.txt
        while [ $iteracion -lt 11 ];
    	    do
	    { time ./blur lobo4k.jpg lobo4kblur.jpg $kernel $threads >/dev/null 2>&1; } 2>> resultados.txt
            iteracion=$[$iteracion+1]
        done
	echo "]," >> resultados.txt
	echo "" >> resultados.txt
	threads=$[$threads*2]
	iteracion=1
    done
    kernel=$[$kernel+2]
    threads=2
    iteracion=1
done
