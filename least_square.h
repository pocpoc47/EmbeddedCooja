#include <math.h>
int least_square(int* y_vals, int size){
    //printf("LEASTSQURE\n");
    int prevision;
    int x_vals[size];
    int i;
    for(i = 0; i<size;i++){
        x_vals[i]=i+1;
    }
    int sum_x=0;
    int sum_y=0;
    for(i=0;i<size;i++){
        sum_x += x_vals[i];
        sum_y += y_vals[i];
    }
    int x_avg=sum_x/size;
    int y_avg=sum_y/size;
    //printf("x_avg: %d\n", x_avg);
    //printf("y_avg: %d\n", y_avg);
    int x_min_avg[size];
    int x_min_avg_sq[size];
    int y_min_avg[size];
    int pro_x_min_avgAND_y_min_avg[size];
    int sum_pro=0;
    int sum_x_min_avg=0;
    for(i=0;i<size;i++){
        //(xi-x_avg)
        x_min_avg[i] = x_vals[i]-x_avg;

        //(xi-x_avg)^2
        x_min_avg_sq[i] = pow(x_vals[i]-x_avg,2);

        //sum(xi-x_avg)
        sum_x_min_avg+=x_min_avg_sq[i];

        //(yi-y_avg)
        y_min_avg[i] = y_vals[i]-y_avg;
              
        //(xi-x_avg)(yi-y_avg)
        pro_x_min_avgAND_y_min_avg[i] = x_min_avg[i]*y_min_avg[i];
        
        //sum(pro)
        sum_pro += pro_x_min_avgAND_y_min_avg[i];
                        

    }
    int a = sum_pro/sum_x_min_avg;
    int b = y_avg -(a*x_avg);
    int x = size+1;
    prevision = a*x + b;
    return prevision;
}
