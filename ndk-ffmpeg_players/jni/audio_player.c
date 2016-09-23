#include <jni.h>
#include <com_ccj_ffmpeg_CcjPlayer.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>


#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"ccj",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"ccj",FORMAT,##__VA_ARGS__);
#include "libyuv.h"

//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//缩放
#include "libswscale/swscale.h"

//重采样
#include "libswresample/swresample.h"

#define MAX_AUDIO_FRME_SIZE 48000 * 4 //一个frame的内存大小

JNIEXPORT void JNICALL Java_com_ccj_ffmpeg_CcjPlayer_sound
(JNIEnv *env, jobject jobj, jstring input_jstr, jstring output_jstr){

	const char* input_cstr = (*env)->GetStringUTFChars(env,input_jstr,NULL);
	const char* output_cstr = (*env)->GetStringUTFChars(env,output_jstr,NULL);
	LOGI("%s","sound");

	 av_register_all(); //注册所有组件。

	 //获取上下文等信息
	 //AVFormatContext 是封装格式上下文结构体,统领全局,保存视频文件的封装格式信息
	 AVFormatContext *pFormCtx=avformat_alloc_context();


	 // Open an input stream and read the header. The codecs are not opened.
	 //* The stream must be closed with avformat_close_input().
	 //打开输入文件。
		if(avformat_open_input(&pFormCtx,input_cstr,NULL,NULL) != 0){
			LOGI("%s","无法打开音频文件");
			return;
		}



	//获取文件信息,
	if(avformat_find_stream_info(pFormCtx,NULL)<0){
		 LOGE("%s","无法获取文件信息");
		 return;
	}

	//获取流索引位置

	int var=0,audio_frame_idx=-1;

	for (; var < pFormCtx->nb_streams; var++) {
		//如果
		//AVStream  pFormatCtx->streams[i] 视频文件中每个视频（音频）流对应一个该结构体。
		//AVCodecContext   pFormatCtx->streams[i]->codec-> 编码器上下文结构体，保存了视频（音频）编解码相关信息。
		if(pFormCtx->streams[var]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
			audio_frame_idx=var;
			break;
		}
	}

	AVStream *avStream=pFormCtx->streams[audio_frame_idx];
	AVCodecContext *avCodeCtx=avStream->codec;
	//获取解码器
	AVCodec *avCode=avcodec_find_decoder(avCodeCtx->codec_id); //查找解码器。

	if(avCode==NULL){
		 LOGE("%s","未找到解码器");
		 return;
	}

	//打开解码器。
	//int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
	if(avcodec_open2(avCodeCtx,avCode,NULL)<0){
		 LOGE("%s","解码器打开失败");
				 return;
	}

	//int av_read_frame(AVFormatContext *s, AVPacket *pkt);

	//AVPacket 存储一帧压缩编码数据。的结构体 需要申请堆中的空间
	AVPacket *avPacket = (AVPacket *)av_malloc(sizeof(AVPacket));

	//AvFrame  存储一帧解码后像素（采样）数据。需要申请空间
	AVFrame *avFrame=av_frame_alloc();



	SwrContext * swrCtx=swr_alloc();

	//采样格式
	enum AVSampleFormat in_sample_fmt=avCodeCtx->sample_fmt;
	//输出采样格式16bit PCM
	enum AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;

	//获取原声音的声道布局.
	int64_t in_ch_layout=avCodeCtx->channel_layout;
	int64_t out_ch_layout=AV_CH_LAYOUT_STEREO;//立体声
	//采样频率
	int out_sample_rate=44100;
	int in_sample_rate=44100;//采样频率

	swr_alloc_set_opts(swrCtx,
	 out_ch_layout,//声音模式 左右声道,立体声等等
	 out_sample_fmt,
	 out_sample_rate,
	 in_ch_layout,
	 in_sample_fmt,
	 in_sample_rate,
	 0,
	 NULL);
	swr_init(swrCtx);

	//16bit 44100 PCM 数据
		uint8_t *out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRME_SIZE);

		FILE *fp_pcm = fopen(output_cstr,"wb");
	int ret;
	//声道的个数
	int dst_nb_channels=	avCodeCtx->channels;

	int got_frame_ptr=0, result=0,index=0;
	 //从输入文件读取一帧压缩数据。
	while(av_read_frame(pFormCtx,avPacket)>=0){

		//将每一帧packet中的数据,写到Frame中
		int result= avcodec_decode_audio4(avCodeCtx,avFrame,&got_frame_ptr,avPacket);
		if(result < 0){
			LOGI("%s","解码完成");
		}

		/*got_frame_ptr 是一个指针,表示是否完成一阵的解码
				Zero if no frame could be decoded, otherwise it is
				no given decode call is guaranteed to produce a frame*/

		if(got_frame_ptr>0){

			 LOGE("正在解码  %d",index++);
			//warning: passing argument 4 of 'swr_convert' from incompatible pointer type [enabled by default]
			swr_convert(swrCtx, &out_buffer, MAX_AUDIO_FRME_SIZE,avFrame->data,avFrame->nb_samples);
			int dst_bufsize = av_samples_get_buffer_size(NULL, dst_nb_channels,avFrame->nb_samples, out_sample_fmt, 1);
			    if (dst_bufsize < 0) {
			    	 LOGE("%s","获取buffer大小失败");
			           return;
			        }
			    fwrite(out_buffer, 1, dst_bufsize, fp_pcm);
		}

		av_free_packet(avPacket);
	}

	fclose(fp_pcm);//释放文件流
	av_frame_free(&avFrame);//释放Frame空间
	av_free(out_buffer);//释放缓冲区
	swr_free(&swrCtx);//释放采样上下文
	avcodec_close(avCodeCtx);//释放解码上下文
	avformat_close_input(&pFormCtx);


	(*env)->ReleaseStringUTFChars(env,input_jstr,input_cstr);
		(*env)->ReleaseStringUTFChars(env,output_jstr,output_cstr);

}

