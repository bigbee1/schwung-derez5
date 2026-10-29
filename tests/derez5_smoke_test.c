#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/dsp/derez5.c"

#define FRAMES 4096
static void log_sink(const char *msg) { (void)msg; }
static void make_signal(int16_t *x) {
    for (int i=0;i<FRAMES;i++) {
        double t=(double)i/44100.0;
        double v=sin(2.0*M_PI*440.0*t)*20000.0 + sin(2.0*M_PI*7000.0*t)*4000.0;
        x[2*i]=x[2*i+1]=(int16_t)v;
    }
}
static double rms_diff(const int16_t *a,const int16_t *b) {
    double sum=0.0;
    for(int i=0;i<FRAMES*2;i++){double d=(double)a[i]-(double)b[i];sum+=d*d;}
    return sqrt(sum/(FRAMES*2));
}
static int render(audio_fx_api_v2_t *api,const char *derez,const char *trim,const char *square,const char *bright,const char *bassis,const char *output,int16_t *out) {
    void *inst=api->create_instance(NULL,"{}"); if(!inst) return 0;
    api->set_param(inst,"derez",derez); api->set_param(inst,"trim",trim); api->set_param(inst,"square",square);
    api->set_param(inst,"bright",bright); api->set_param(inst,"bassis",bassis); api->set_param(inst,"output",output);
    make_signal(out); api->process_block(inst,out,FRAMES); api->destroy_instance(inst); return 1;
}
int main(void) {
    host_api_v1_t host={0}; host.log=log_sink;
    audio_fx_api_v2_t *api=move_audio_fx_init_v2(&host);
    if(!api || api->api_version!=2){fprintf(stderr,"FAIL: API v2 init\n");return 1;}
    void *inst=api->create_instance(NULL,"{}"); char value[64];
    if(!inst || api->get_param(inst,"derez",value,sizeof(value))<0 || strcmp(value,"0.5000")) {fprintf(stderr,"FAIL: audible default, got %s\n",inst?value:"no instance");return 1;}
    api->destroy_instance(inst);
    int16_t baseline[FRAMES*2], variant[FRAMES*2], original[FRAMES*2];
    make_signal(original);
    if(!render(api,"0.5","1","0","1","0","1",baseline)){return 1;}
    if(rms_diff(baseline,original)<100.0){fprintf(stderr,"FAIL: default DSP does not alter input\n");return 1;}
    const char *names[]={"trim","square","bright","bassis","output"};
    const char *vals[][6]={{"0.5","0","0","1","0","1"},{"0.5","1","1","1","0","1"},{"0.5","1","0","0.5","0","1"},{"0.5","1","0","1","0.8","1"},{"0.5","1","0","1","0","0.25"}};
    for(int i=0;i<5;i++){
        if(!render(api,vals[i][0],vals[i][1],vals[i][2],vals[i][3],vals[i][4],vals[i][5],variant))return 1;
        double d=rms_diff(baseline,variant);
        if(d<10.0){fprintf(stderr,"FAIL: %s control had no measurable DSP effect (%.2f)\n",names[i],d);return 1;}
        printf("PASS: %s changes audio (RMS difference %.2f)\n",names[i],d);
    }
    printf("PASS: Derez5 DSP changes audio at default Derez=0.5; all tested controls affect output.\n");
    return 0;
}
