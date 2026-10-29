/*
 * DeRez5 for Schwung / Ableton Move
 * DSP port based on Airwindows DeRez5 (Chris Johnson), MIT license.
 * Original algorithm: https://github.com/airwindows/airwindows
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "plugin_api_v1.h"

#define API_VERSION 2
#define SR 44100.0

enum { HMAL, HMBL, HMCL, HMDL, LMAL, LMBL, LMCL, LMDL,
       HMAR, HMBR, HMCR, HMDR, HMCYCLE, LMAR, LMBR, LMCR, LMDR, LMCYCLE, BEZ_TOTAL };

typedef struct {
    float derez, trim, square, bright, bassis, output;
    double bez[BEZ_TOTAL][2];
    double pointCycleL, pointAL, pointBL, sDerezL;
    double pointCycleR, pointAR, pointBR, sDerezR;
    double lastSampleL, directionCountL, lastSampleR, directionCountR;
    int goingUpL, goingUpR;
    uint32_t fpdL, fpdR;
} derez5_t;

typedef struct {
    uint32_t api_version;
    void* (*create_instance)(const char*, const char*);
    void (*destroy_instance)(void*);
    void (*process_block)(void*, int16_t*, int);
    void (*set_param)(void*, const char*, const char*);
    int (*get_param)(void*, const char*, char*, int);
} audio_fx_api_v2_t;
static const host_api_v1_t *g_host;
static float clampf(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static uint32_t rng32(uint32_t *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static void init_state(derez5_t *s) {
    memset(s->bez, 0, sizeof(s->bez));
    s->pointCycleL=s->pointAL=s->pointBL=0; s->sDerezL=1;
    s->pointCycleR=s->pointAR=s->pointBR=0; s->sDerezR=1;
    s->lastSampleL=s->lastSampleR=0; s->directionCountL=s->directionCountR=1;
    s->goingUpL=s->goingUpR=0;
    s->fpdL=0x1234567u; s->fpdR=0x7654321u;
}
static void *create_instance(const char *dir, const char *json) {
    (void)dir; (void)json;
    derez5_t *s=calloc(1,sizeof(*s)); if(!s) return NULL;
    s->derez=0.50f; s->trim=1.0f; s->square=0.0f; s->bright=1.0f; s->bassis=0.0f; s->output=1.0f;
    init_state(s); return s;
}
static void destroy_instance(void *p) { free(p); }
static void set_param(void *p,const char *key,const char *val) {
    derez5_t *s=p; if(!s||!key||!val) return;
    float v=clampf((float)atof(val));
    if(!strcmp(key,"derez")) s->derez=v;
    else if(!strcmp(key,"trim")) s->trim=v;
    else if(!strcmp(key,"square")) s->square=v;
    else if(!strcmp(key,"bright")) s->bright=v;
    else if(!strcmp(key,"bassis")) s->bassis=v;
    else if(!strcmp(key,"output")) s->output=v;
    else if(!strcmp(key,"state")) {
        const char *keys[]={"derez","trim","square","bright","bassis","output"};
        float *dst[]={&s->derez,&s->trim,&s->square,&s->bright,&s->bassis,&s->output};
        for(int i=0;i<6;i++){ char pat[32]; snprintf(pat,sizeof(pat),"\"%s\"",keys[i]); const char *q=strstr(val,pat); if(q&&(q=strchr(q,':'))) *dst[i]=clampf((float)atof(q+1)); }
    }
}
static int get_param(void *p,const char *key,char *buf,int n) {
    derez5_t *s=p; if(!s||!key||!buf||n<1) return -1;
    if(!strcmp(key,"name")) return snprintf(buf,n,"DeRez5");
    if(!strcmp(key,"derez")) return snprintf(buf,n,"%.4f",s->derez);
    if(!strcmp(key,"trim")) return snprintf(buf,n,"%.4f",s->trim);
    if(!strcmp(key,"square")) return snprintf(buf,n,"%.4f",s->square);
    if(!strcmp(key,"bright")) return snprintf(buf,n,"%.4f",s->bright);
    if(!strcmp(key,"bassis")) return snprintf(buf,n,"%.4f",s->bassis);
    if(!strcmp(key,"output")) return snprintf(buf,n,"%.4f",s->output);
    if(!strcmp(key,"state")) return snprintf(buf,n,"{\"derez\":%.4f,\"trim\":%.4f,\"square\":%.4f,\"bright\":%.4f,\"bassis\":%.4f,\"output\":%.4f}",s->derez,s->trim,s->square,s->bright,s->bassis,s->output);
    if(!strcmp(key,"ui_hierarchy")) return snprintf(buf,n,"{\"modes\":null,\"levels\":{\"root\":{\"children\":null,\"knobs\":[\"derez\",\"trim\",\"square\",\"bright\",\"bassis\",\"output\"],\"params\":[\"derez\",\"trim\",\"square\",\"bright\",\"bassis\",\"output\"]}}}");
    if(!strcmp(key,"chain_params")) return snprintf(buf,n,"[{\"key\":\"derez\",\"name\":\"Derez\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},{\"key\":\"trim\",\"name\":\"Trim\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},{\"key\":\"square\",\"name\":\"Square\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},{\"key\":\"bright\",\"name\":\"Bright\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},{\"key\":\"bassis\",\"name\":\"Bassis\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01},{\"key\":\"output\",\"name\":\"Output\",\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01}]");
    return -1;
}
static void process_block(void *p,int16_t *audio,int frames) {
    derez5_t *s=p; if(!s||!audio) return;
    double overallscale=1.0; overallscale=overallscale/44100.0*SR;
    double applyDerez=1.0-pow(s->derez,2.0+overallscale);
    double trimDerez=(1.0-s->trim)*16.0*overallscale;
    double square=1.0-pow(s->square,3.0);
    double brightBez=(1.0-pow(s->bright,2.0))*4.0*overallscale;
    int stepped=999999;
    if(brightBez>1.0){stepped=(int)brightBez; brightBez=0.99999999/stepped;} else brightBez=1.1;
    double brightTrim=1.0-(brightBez*((double)stepped/(stepped+1.0)));
    double bassRef=pow(s->bassis,4.0);
    double bassisBez=(bassRef*0.25)/overallscale;
    stepped=999999; if(bassisBez>0.000001) stepped=(int)(1.0/bassisBez);
    if(bassisBez>0.0) bassisBez=0.99999999/stepped;
    double bassisTrim=1.0-(bassisBez*((double)stepped/(stepped+1.0)));
    double output=s->output;
    for(int i=0;i<frames;i++) {
        double inL=(double)audio[2*i]/32768.0, inR=(double)audio[2*i+1]/32768.0;
        if(fabs(inL)<1.18e-23) inL=s->fpdL*1.18e-17;
        if(fabs(inR)<1.18e-23) inR=s->fpdR*1.18e-17;
        if ((inL>s->lastSampleL)==s->goingUpL) s->directionCountL+=(applyDerez+trimDerez); else s->directionCountL=trimDerez+1.0;
        s->goingUpL=(inL>s->lastSampleL); s->lastSampleL=inL;
        if ((inR>s->lastSampleR)==s->goingUpR) s->directionCountR+=(applyDerez+trimDerez); else s->directionCountR=trimDerez+1.0;
        s->goingUpR=(inR>s->lastSampleR); s->lastSampleR=inR;
        if(brightBez<1.0) {
            s->bez[HMCYCLE][0]+=brightBez; s->bez[HMAL][0]+=inL*0.5*brightBez; s->bez[HMAR][0]+=inR*0.5*brightBez;
            if(s->bez[HMCYCLE][0]>brightTrim){s->bez[HMDL][0]=s->bez[HMCL][0];s->bez[HMCL][0]=s->bez[HMBL][0];s->bez[HMBL][0]=s->bez[HMAL][0]*(0.5-brightBez*0.082);s->bez[HMAL][0]=0;s->bez[HMDR][0]=s->bez[HMCR][0];s->bez[HMCR][0]=s->bez[HMBR][0];s->bez[HMBR][0]=s->bez[HMAR][0]*(0.5-brightBez*0.082);s->bez[HMAR][0]=0;s->bez[HMCYCLE][0]=0;}
            double X=s->bez[HMCYCLE][0]; inL=s->bez[HMCL][0]+s->bez[HMDL][0]*(1-X)*(1-X)+s->bez[HMCL][0]*2*(1-X)*X+s->bez[HMBL][0]*X*X; inL/=brightTrim;
            inR=s->bez[HMCR][0]+s->bez[HMDR][0]*(1-X)*(1-X)+s->bez[HMCR][0]*2*(1-X)*X+s->bez[HMBR][0]*X*X; inR/=brightTrim;
        }
        if(bassisBez>0) {
            s->bez[LMCYCLE][0]+=bassisBez;s->bez[LMAL][0]+=inL*bassisBez;s->bez[LMAR][0]+=inR*bassisBez;
            if(s->bez[LMCYCLE][0]>bassisTrim){s->bez[LMDL][0]=s->bez[LMCL][0];s->bez[LMCL][0]=s->bez[LMBL][0];s->bez[LMBL][0]=s->bez[LMAL][0]*(0.5-bassisBez*0.082);s->bez[LMAL][0]=0;s->bez[LMDR][0]=s->bez[LMCR][0];s->bez[LMCR][0]=s->bez[LMBR][0];s->bez[LMBR][0]=s->bez[LMAR][0]*(0.5-bassisBez*0.082);s->bez[LMAR][0]=0;s->bez[LMCYCLE][0]=0;}
            double X=s->bez[LMCYCLE][0]; inL-=(s->bez[LMCL][0]+s->bez[LMDL][0]*(1-X)*(1-X)+s->bez[LMCL][0]*2*(1-X)*X+s->bez[LMBL][0]*X*X)*0.5; inL/=bassisTrim;
            inR-=(s->bez[LMCR][0]+s->bez[LMDR][0]*(1-X)*(1-X)+s->bez[LMCR][0]*2*(1-X)*X+s->bez[LMBR][0]*X*X)*0.5; inR/=bassisTrim;
        }
        if(applyDerez+trimDerez>0) {
            s->pointCycleL+=s->sDerezL;
            if(s->pointCycleL>0.9999999){s->pointAL=s->pointBL;s->pointBL=inL;s->pointCycleL=0;stepped=999999;if(applyDerez<0.999999)stepped=(int)(1.0/(1.0-applyDerez));if(stepped>s->directionCountL)stepped=(int)s->directionCountL;if(stepped<1)stepped=1;s->sDerezL=0.99999999/stepped;}
            double X=s->pointCycleL*square; inL=s->sDerezL>0?s->pointAL+(s->pointBL-s->pointAL)*X:0;
            s->pointCycleR+=s->sDerezR;
            if(s->pointCycleR>0.9999999){s->pointAR=s->pointBR;s->pointBR=inR;s->pointCycleR=0;stepped=999999;if(applyDerez<0.999999)stepped=(int)(1.0/(1.0-applyDerez));if(stepped>s->directionCountR)stepped=(int)s->directionCountR;if(stepped<1)stepped=1;s->sDerezR=0.99999999/stepped;}
            X=s->pointCycleR*square; inR=s->sDerezR>0?s->pointAR+(s->pointBR-s->pointAR)*X:0;
        }
        if(brightBez<1.0) {
            s->bez[HMCYCLE][1]+=brightBez;s->bez[HMAL][1]+=inL*brightBez;s->bez[HMAR][1]+=inR*brightBez;
            if(s->bez[HMCYCLE][1]>brightTrim){s->bez[HMDL][1]=s->bez[HMCL][1];s->bez[HMCL][1]=s->bez[HMBL][1];s->bez[HMBL][1]=s->bez[HMAL][1]*(0.5-brightBez*0.082);s->bez[HMAL][1]=0;s->bez[HMDR][1]=s->bez[HMCR][1];s->bez[HMCR][1]=s->bez[HMBR][1];s->bez[HMBR][1]=s->bez[HMAR][1]*(0.5-brightBez*0.082);s->bez[HMAR][1]=0;s->bez[HMCYCLE][1]=0;}
            double X=s->bez[HMCYCLE][1]; inL=(s->bez[HMCL][1]+s->bez[HMDL][1]*(1-X)*(1-X)+s->bez[HMCL][1]*2*(1-X)*X+s->bez[HMBL][1]*X*X)/brightTrim;
            inR=(s->bez[HMCR][1]+s->bez[HMDR][1]*(1-X)*(1-X)+s->bez[HMCR][1]*2*(1-X)*X+s->bez[HMBR][1]*X*X)/brightTrim;
        }
        if(bassisBez>0) {
            s->bez[LMCYCLE][1]+=bassisBez;s->bez[LMAL][1]+=inL*bassisBez;s->bez[LMAR][1]+=inR*bassisBez;
            if(s->bez[LMCYCLE][1]>bassisTrim){s->bez[LMDL][1]=s->bez[LMCL][1];s->bez[LMCL][1]=s->bez[LMBL][1];s->bez[LMBL][1]=s->bez[LMAL][1]*(0.5-bassisBez*0.082);s->bez[LMAL][1]=0;s->bez[LMDR][1]=s->bez[LMCR][1];s->bez[LMCR][1]=s->bez[LMBR][1];s->bez[LMBR][1]=s->bez[LMAR][1]*(0.5-bassisBez*0.082);s->bez[LMAR][1]=0;s->bez[LMCYCLE][1]=0;}
            double X=s->bez[LMCYCLE][1]; inL-=(s->bez[LMCL][1]+s->bez[LMDL][1]*(1-X)*(1-X)+s->bez[LMCL][1]*2*(1-X)*X+s->bez[LMBL][1]*X*X)*0.5;inL/=bassisTrim;
            inR-=(s->bez[LMCR][1]+s->bez[LMDR][1]*(1-X)*(1-X)+s->bez[LMCR][1]*2*(1-X)*X+s->bez[LMBR][1]*X*X)*0.5;inR/=bassisTrim;
        }
        inL*=output; inR*=output;
        rng32(&s->fpdL); rng32(&s->fpdR);
        if(inL>1) inL=1;
        if(inL< -1) inL=-1;
        if(inR>1) inR=1;
        if(inR< -1) inR=-1;
        audio[2*i]=(int16_t)lrint(inL*32767.0); audio[2*i+1]=(int16_t)lrint(inR*32767.0);
    }
}
static audio_fx_api_v2_t api;
__attribute__((visibility("default"))) audio_fx_api_v2_t *move_audio_fx_init_v2(const host_api_v1_t *host) {
    g_host=host; api.api_version=API_VERSION; api.create_instance=create_instance; api.destroy_instance=destroy_instance; api.process_block=process_block; api.set_param=set_param; api.get_param=get_param;
    if(g_host&&g_host->log) g_host->log("[derez5] Airwindows DeRez5 initialized");
    return &api;
}
