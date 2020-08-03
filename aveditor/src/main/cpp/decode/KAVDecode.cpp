//
// Created by 阳坤 on 2020-05-22.
//

#include "KAVDecode.h"

void KAVDecode::initMediaCodec(void *vm) {
    av_jni_set_java_vm(vm, 0);
}


AVCodec *getCodec(AVCodecParameters *parameters) {
    AVCodec *codec;
    switch (parameters->codec_id) {
        case AV_CODEC_ID_H264:
            codec = avcodec_find_decoder_by_name(H264_MEDIACODEC);//硬解码264
            if (codec == NULL) {
                LOGE("Couldn't find Codec. H264_MEDIACODEC\n");
                return NULL;
            }
            break;
        case AV_CODEC_ID_MPEG4:
            codec = avcodec_find_decoder_by_name(MPEG4_MEDIACODEC);//硬解码mpeg4
            if (codec == NULL) {
                LOGE("Couldn't find Codec. MPEG4_MEDIACODEC\n");
                return NULL;
            }
            break;
        case AV_CODEC_ID_HEVC:
            codec = avcodec_find_decoder_by_name(HEVC_MEDIACODEC);//硬解码265
            if (codec == NULL) {
                LOGE("Couldn't find Codec.HEVC_MEDIACODEC \n");
                return NULL;
            }
            break;
        default:
            codec = avcodec_find_decoder(parameters->codec_id);//软解
            if (codec == NULL) {
                LOGE("Couldn't find Codec.\n");
                return NULL;
            }
            break;
    }
    return codec;
}

int KAVDecode::open(AVParameter par, int isMediaCodec) {
    int ret = 0;
    //打开之前先清理之前预留的资源
    close();
    //保证解码参数不为空
    if (!par.para)return ret;
    mux.lock();
    //拿到解码参数
    AVCodecParameters *parameters = par.para;
    //先根据解码参数的 id 找到解码器
    AVCodec *codec = avcodec_find_decoder(parameters->codec_id);
    //支持硬件解码
    if (isMediaCodec) {
        codec = getCodec(parameters);
    }
    if (!codec) {
        mux.unlock();
        close();
        LOGE("avcodec_find_decoder %d failed!  %d", parameters->codec_id, isMediaCodec);
        return false;
    }
    LOGI("avcodec_find_decoder  success !  isMediacodec:%d", isMediaCodec);
    //创建解码器上下文
    this->pCodec = avcodec_alloc_context3(codec);
    //将解码器设置解码参数
    ret = avcodec_parameters_to_context(this->pCodec, parameters);
    if (ret < 0) {
        mux.unlock();
        close();
        LOGE("avcodec_parameters_to_context error! %d", ret);
        return false;
    }
    //指定多线程解码数量
    pCodec->thread_count = 4;
    //打开解码器
    ret = avcodec_open2(pCodec, codec, 0);
    if (ret != 0) {
        char buf[1024] = {0};
        av_strerror(ret, buf, sizeof(buf) - 1);
        LOGE("%s", buf);
        mux.unlock();
    }
    if (pCodec->codec_type == AVMEDIA_TYPE_VIDEO) {
        this->isAudio = false;
    } else {
        this->isAudio = true;
    }
    LOGI("avcodec_open2 open success! 是否是音频解码器: %d", this->isAudio);
    mux.unlock();
    return ret;
}

int KAVDecode::close() {
    IDecode::clear();
    mux.lock();
    pts = 0;
    if (pFrame)
        av_frame_free(&pFrame);
    if (pCodec) {
        avcodec_close(pCodec);
        avcodec_free_context(&pCodec);
    }
    mux.unlock();
    return 1;
}

int KAVDecode::clear() {
    IDecode::clear();
    mux.lock();
    if (pCodec)
        avcodec_flush_buffers(pCodec);
    mux.unlock();
    return true;
}

int KAVDecode::sendPacket(AVData data) {
    //判断是否是空数据。
    if (!data.data || data.size <= 0)return 0;
    mux.lock();
    if (!pCodec) {
        mux.unlock();
        return false;
    }
    int ret = avcodec_send_packet(pCodec, reinterpret_cast<const AVPacket *>(data.data));
    mux.unlock();
    return ret == 0 ? 1 : 0;
}

AVData KAVDecode::getDecodeFrame() {
    mux.lock();
    if (!pCodec) {
        mux.unlock();
        return AVData();
    }
    if (!pFrame)
        pFrame = av_frame_alloc();

    //拿到解码之后的数据 PCM/H264 0 is ok.
    int ret = avcodec_receive_frame(pCodec, pFrame);
    if (ret != 0) {
        mux.unlock();
        char buf[1024] = {0};
        av_strerror(ret, buf, sizeof(buf) - 1);
        LOGE("avcodec_receive_frame error %s", buf);
        return AVData();
    }

    AVData deData;
    deData.data = (unsigned char *) pFrame;
    if (pCodec->codec_type == AVMEDIA_TYPE_VIDEO) {
        deData.size = (pFrame->linesize[0] + pFrame->linesize[1] + pFrame->linesize[2]) * pFrame->height;
        deData.width = pFrame->width;
        deData.height = pFrame->height;
        deData.isAudio = 0;
//        LOGE("解码成功! AVMEDIA_TYPE_VIDEO");
    } else if (pCodec->codec_type == AVMEDIA_TYPE_AUDIO) {
        //样本字节数 * 单通道样本数 * 通道数
        deData.size = av_get_bytes_per_sample((AVSampleFormat) pFrame->format) * pFrame->nb_samples * 2;
        deData.isAudio = 1;
//        LOGE("解码成功! AVMEDIA_TYPE_AUDIO");
    } else {
        mux.unlock();
//        LOGE("解码成功! UNKOWN");
        return AVData();
    }
    deData.format = pFrame->format;
    //if(!isAudio)
    //    XLOGE("data format is %d",frame->format);
    memcpy(deData.datas, pFrame->data, sizeof(deData.datas));
    deData.pts = pFrame->pts;
    pts = deData.pts;
    mux.unlock();
    return deData;
}
