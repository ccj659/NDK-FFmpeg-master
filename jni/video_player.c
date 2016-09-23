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

/*
 * Class:     com_ccj_ffmpeg_CcjPlayer
 * Method:    render
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_com_ccj_ffmpeg_CcjPlayer_render
(JNIEnv *env, jobject jobj, jstring input_jstr, jobject surface){
	const char* input_cstr = (*env)->GetStringUTFChars(env,input_jstr,NULL);
	//1.注册组件
	av_register_all();

	//封装格式上下文
	AVFormatContext *pFormatCtx = avformat_alloc_context();

	//2.打开输入视频文件
	if(avformat_open_input(&pFormatCtx,input_cstr,NULL,NULL) != 0){
		LOGE("%s","打开输入视频文件失败");
		return;
	}
	//3.获取视频信息
	if(avformat_find_stream_info(pFormatCtx,NULL) < 0){
		LOGE("%s","获取视频信息失败");
		return;
	}

	//视频解码，需要找到视频对应的AVStream所在pFormatCtx->streams的索引位置
	int video_stream_idx = -1;
	int i = 0;
	for(; i < pFormatCtx->nb_streams;i++){
		//根据类型判断，是否是视频流
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			video_stream_idx = i;
			break;
		}
	}

	//4.获取视频解码器
	AVCodecContext *pCodeCtx = pFormatCtx->streams[video_stream_idx]->codec;
	AVCodec *pCodec = avcodec_find_decoder(pCodeCtx->codec_id);
	if(pCodec == NULL){
		LOGE("%s","无法解码");
		return;
	}

	//5.打开解码器
	if(avcodec_open2(pCodeCtx,pCodec,NULL) < 0){
		LOGE("%s","解码器无法打开");
		return;
	}

	//编码数据
	AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));

	//像素数据（解码数据）
	AVFrame *yuv_frame = av_frame_alloc();
	AVFrame *rgb_frame = av_frame_alloc();

	//ScaleImg視頻縮放
	if(ScaleImg(pCodeCtx,yuv_frame,rgb_frame,pCodeCtx->width/6,pCodeCtx->width/6)!=1){
		LOGE("%s","縮放失敗");
		return;
	}

	//native绘制
	//窗体

	ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env,surface);

	//绘制时的缓冲区
	ANativeWindow_Buffer outBuffer;

	int len ,got_frame, framecount = 0;
	//6.一阵一阵读取压缩的视频数据AVPacket
	while(av_read_frame(pFormatCtx,packet) >= 0){
		//解码AVPacket->AVFrame
		len = avcodec_decode_video2(pCodeCtx, yuv_frame, &got_frame, packet);

		//Zero if no frame could be decompressed
		//非零，正在解码
		if(got_frame){
			LOGI("解码%d帧",framecount++);
			//lock
			//设置缓冲区的属性（宽、高、像素格式）
			ANativeWindow_setBuffersGeometry(nativeWindow, pCodeCtx->width, pCodeCtx->height,WINDOW_FORMAT_RGBA_8888);
			ANativeWindow_lock(nativeWindow,&outBuffer,NULL);

			//设置rgb_frame的属性（像素格式、宽高）和缓冲区
			//rgb_frame缓冲区与outBuffer.bits是同一块内存
			avpicture_fill((AVPicture *)rgb_frame, outBuffer.bits, AV_PIX_FMT_RGBA, pCodeCtx->width, pCodeCtx->height);

			//YUV->RGBA_8888
			I420ToARGB(yuv_frame->data[0],yuv_frame->linesize[0],
					yuv_frame->data[2],yuv_frame->linesize[2],
					yuv_frame->data[1],yuv_frame->linesize[1],
					rgb_frame->data[0], rgb_frame->linesize[0],
					pCodeCtx->width,pCodeCtx->height);

			//unlock
			ANativeWindow_unlockAndPost(nativeWindow);

			usleep(1000 * 16);

		}

		av_free_packet(packet);
	}

	ANativeWindow_release(nativeWindow);
	av_frame_free(&yuv_frame);
	avcodec_close(pCodeCtx);
	avformat_free_context(pFormatCtx);

	(*env)->ReleaseStringUTFChars(env,input_jstr,input_cstr);
}



int ScaleImg(AVCodecContext *pCodecCtx,AVFrame *src_picture,AVFrame *dst_picture,int nDstH ,int nDstW )
{
	int i ;
	int nSrcStride[3];
	int nDstStride[3];
	int nSrcH = pCodecCtx->height;
	int nSrcW = pCodecCtx->width;
	struct SwsContext* m_pSwsContext;


	uint8_t *pSrcBuff[3] = {src_picture->data[0],src_picture->data[1], src_picture->data[2]};


	nSrcStride[0] = nSrcW ;
	nSrcStride[1] = nSrcW/2 ;
	nSrcStride[2] = nSrcW/2 ;




	dst_picture->linesize[0] = nDstW;
	dst_picture->linesize[1] = nDstW / 2;
	dst_picture->linesize[2] = nDstW / 2;


	printf("nSrcW%d\n",nSrcW);


	m_pSwsContext = sws_getContext(nSrcW, nSrcH, PIX_FMT_YUV420P10,
	nDstW, nDstH, PIX_FMT_YUV420P10,
	SWS_BICUBIC,
	NULL, NULL, NULL);


	if (NULL == m_pSwsContext)
	{
	printf("ffmpeg get context error!\n");
	exit (-1);
	}


	sws_scale(m_pSwsContext, src_picture->data,src_picture->linesize, 0, pCodecCtx->height,dst_picture->data,dst_picture->linesize);


	printf("line0:%d line1:%d line2:%d\n",dst_picture->linesize[0] ,dst_picture->linesize[1] ,dst_picture->linesize[2]);
	sws_freeContext(m_pSwsContext);

	LOGE("%s","縮放成功");
	return 1 ;
}

