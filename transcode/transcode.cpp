/*
 * 视频转编码例子，h264转h265例子
 * The sample of transcoding video, h264 to h265 example
 * Depends on FFmpeg 6.0
 * Wirte by stoprefactoring.com
*/

#include <iostream>
extern "C" {  
    #include <libavutil/timestamp.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/codec.h>
}

int ret = 0;

//输入输出文件路径
//Input and output file paths
const char *inFilePath  = "../../common/test.mp4";
//const char *inFilePath  = "rtmp://192.168.3.202:1935/live/test";
const char *outFilePath  = "./out.mp4";

//视频目标编码
//Video target encoding
const AVCodecID videoCodecID = AV_CODEC_ID_H265;
//音频目标编码
//音频编码一般有固定的frameSize，如AAC是1024个采样是一帧，MP3是1052个采样是一帧
//但是单纯的转编码，不会改变音频帧的原始数据
//如果单纯采用转编码，将AAC转MP3，编码器是不会自动将一帧1024个采样改为1052个采样的，而是会报错退出
//所以这里按原音频编码格式输出，但是程序仍然会进行完整的解码/编码过程
//Audio target encoding
//Audio encoding generally has a fixed frameSize, such as AAC is 1024 samples is a frame, MP3 is 1052 samples is a frame.
//But pure transcoding will not change the original data of the audio frame.
//If you simply use transcoding to convert AAC to MP3, the encoder will not automatically change a frame from 1024 samples to 1052 samples, but will report an error and exit.
//So here the output is in the original audio encoding format, but the program still performs the full decoding/encoding process.
const AVCodecID audioCodecID = AV_CODEC_ID_AAC;

//输入输出文件句柄
//Input and output file handles
AVFormatContext *inFileHandle = NULL;
AVFormatContext *outFileHandle = NULL;

//轨道上下文结构体，存放解码器、编码器、输出轨道序号、轨道类型等
//Track context structure, inlcude the decoder, encoder, output track number, track type
typedef struct StreamContext {
    AVMediaType type;                                                           //轨道类型，track type
    unsigned int outIndex;                                                      //输出轨道序号，output track number
    AVCodecContext *decoder;                                                    //解码器，decoder
    AVCodecContext *encoder;                                                    //编码器，encoder
    bool isDecodeEnd;                                                           //解码器处理完毕标志，decode end
    bool isEncodeEnd;                                                           //编码器处理完毕标志，encode end
} StreamContext;
//轨道上下文关联表
//Stream context correlation table
StreamContext *streamContextMapping = NULL;
int streamContextLength = 0;

void termination(const char* param){
    std::cout<<param<<std::endl;
    std::cout<<"Error occur, quit!"<<std::endl;
    exit(-1);
}

void Step_OpenInFile(){
    //STEP::打开源视频文件
    //STEP::Open the input video file
    AVDictionary* optionsDict = NULL;                                                 //设置输入源封装参数
    av_dict_set(&optionsDict, "rw_timeout", "2000000", 0);                            //设置网络超时，当输入源为文件时，可注释此行。Set the network timeout, you can comment out this line when the input source is a file
    ret = avformat_open_input(&inFileHandle, inFilePath, NULL, &optionsDict);
    if(ret<0){
        termination("Could not open input file.");
    }

    //STEP::获取源视频文件的流信息
    //STEP::Get the stream information of the source video file
    ret = avformat_find_stream_info(inFileHandle, NULL);
    if(ret<0){
        termination("Failed to retrieve input stream information.");
    }

    //STEP::根据源轨道信息创建streamContextMapping
    //STEP::Create streamContextMapping based on source track information
    streamContextLength = inFileHandle->nb_streams;
    streamContextMapping = (StreamContext *)av_malloc_array(streamContextLength, sizeof(*streamContextMapping));
    if(!streamContextMapping){
        streamContextLength = 0;
        termination("Could not allocate stream context mapping.");
    }
    for(int i=0;i<streamContextLength;i++){
        streamContextMapping[i].type = inFileHandle->streams[i]->codecpar->codec_type;
        streamContextMapping[i].outIndex = -1;
        streamContextMapping[i].decoder = NULL;
        streamContextMapping[i].encoder = NULL;
        streamContextMapping[i].isDecodeEnd = false;
        streamContextMapping[i].isEncodeEnd = false;
    }
}

void Step_CreateOutFile(){
    //STEP::创建输出文件句柄outFileHandle
    //STEP::Creates an output file handle, outFileHandle.
    ret = avformat_alloc_output_context2(&outFileHandle, NULL, NULL, outFilePath);
    if(ret<0){
        termination("Could not create output handle.");
    }

    //设置输出的格式，如下设置等于ffmpeg ... -f flv rtmp://xxx
    //Set the format of the output, the following settings are equal to ffmpeg ... -f flv rtmp://xxx
    //avformat_alloc_output_context2(&outFileHandle, NULL, "flv", outFilePath);

    //STEP::根据源轨道信息创建输出文件的音视频轨道
    //由于仅做转封装是无法改变音视频数据的，所以轨道信息只能复制
    //STEP::Create audio/video tracks for output files based on source track information
    //Since it is not possible to change the audio and video data by only doing remuxing, the track information can only be copied
    unsigned int outStreamIndex = 0;
    for(unsigned int i = 0; i < streamContextLength; i++) {
        AVStream *inStream = inFileHandle->streams[i];
        if (streamContextMapping[i].type != AVMEDIA_TYPE_AUDIO &&                                  //过滤除video、audio、subtitle以外的轨道，Filter tracks except video, audio, subtitle
            streamContextMapping[i].type != AVMEDIA_TYPE_VIDEO &&
            streamContextMapping[i].type != AVMEDIA_TYPE_SUBTITLE) {
                streamContextMapping[i].outIndex = -1;
                continue;
        }

        AVStream *outStream = avformat_new_stream(outFileHandle, NULL);                            //创建输出的轨道，Creating the output track
        if(streamContextMapping[i].encoder){                                                       //尝试从编码器复制输出轨道信息，Trying to copy the output track information from the encoder
            ret = avcodec_parameters_from_context(outStream->codecpar, streamContextMapping[i].encoder);
        }else{
            ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);                //复制源轨道的信息到输出轨道，Copying information from the source track to the output track
        }
        if(ret<0){
            termination("Could not copy codec parameters.");
        }
        outStream->codecpar->codec_tag = 0;
        streamContextMapping[i].outIndex = outStreamIndex++;                                       //记录源文件轨道序号与输出文件轨道序号的对应关系，Record the correspondence between the track number of the source file and the track number of the output file.
    }

    //STEP::打开输出文件
    //STEP::Open the output file
    ret = avio_open(&outFileHandle->pb, outFilePath, AVIO_FLAG_WRITE);
    if(ret<0){
        termination("Could not open out file.");
    }

    //STEP::写入文件头信息
    //STEP::Write file header information
    ret = avformat_write_header(outFileHandle, NULL);
    if(ret<0){
        termination("Could not write stream header to out file.");
    }

    // 设置输出封装参数的方式，如hls的hls_time参数
    // The way to set the output muxing parameters, such as the hls_time parameter of hls
    // AVDictionary* optionsDict = NULL;
    // av_dict_set(&optionsDict, "hls_time", "2", 0);
    // avformat_write_header(outFileHandle, &optionsDict);
}

void Step_OpenDecoder(){
    //STEP::初始化轨道上下文列表
    //STEP::Initialize stream context list
    if(streamContextLength == 0){
        return;
    }

    //STEP::根据输入文件的流信息创建解码器
    //STEP::Create decoders based on the stream information of the input file
    for(int i=0;i<streamContextLength;i++){
        AVStream *inStream = inFileHandle->streams[i];
        if (inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&                             //过滤除video、audio以外的轨道，Filter tracks except video, audio
            inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO ) {
            continue;
        }

        const AVCodec *decoderInfo = avcodec_find_decoder(inStream->codecpar->codec_id);       //根据输入文件的流信息寻找解码器，Find the decoder based on the stream information of the input file
        if(!decoderInfo){
            termination("Could not find decoder for stream.");
        }

        AVCodecContext *decoder = avcodec_alloc_context3(decoderInfo);                          //创建解码器上下文，Create decoder context
        if(!decoder){
            termination("Could not allocate decoder context.");
        }

        ret = avcodec_parameters_to_context(decoder, inStream->codecpar);                       //从流信息拷贝参数到解码器上下文，Copy parameters from the stream information to the decoder context
        if(ret<0){
            termination("Could not copy parameters from the stream information to the decoder context.");
        }

        if(inStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            decoder->framerate = av_guess_frame_rate(inFileHandle, inStream, NULL);
        }

        decoder->time_base = AV_TIME_BASE_Q;                                                    //固定TimeBase为1/1000000，能防止能多奇怪问题，Fixed TimeBase is 1/1000000，can prevent strange problems

        AVDictionary *optionsDict = NULL;
        //av_dict_set(&optionsDict, "threads", "2", 0);                                         //可设置解码器的一些参数，如处理线程数，Set some parameters of the decoder, such as the number of processing threads
        ret = avcodec_open2(decoder, decoderInfo, &optionsDict);
        if(ret<0){
            termination("Could not open decoder.");
        }

        decoder->time_base = AV_TIME_BASE_Q;                                                    //一些编码器会修改timebase，这里做一次覆盖设置，Some encoders modify timebase, do an override setting here
        streamContextMapping[i].decoder = decoder;
        streamContextMapping[i].type = inStream->codecpar->codec_type;
    }
}

void Step_OpenEncoder(){
    //STEP::根据解码器创建编码器，因为单纯的转编码无法改变音视频基础参数，如分辨率、采样率等，所以这些参数只能复制
    //STEP::Create encoders based on decoders, since it is not possible to change the audio and video data by only doing transcoding, the basic parameters such as resolution, sample rate, etc. can only be copied
    for(int i=0;i<streamContextLength;i++){
        if(!streamContextMapping[i].decoder){
            continue;
        }

        const AVCodec *encoderInfo = NULL;
        if (streamContextMapping[i].type == AVMEDIA_TYPE_VIDEO){
            encoderInfo = avcodec_find_encoder(videoCodecID);                                  //根据目标编码器ID寻找编码器，Find the encoder based on the target encoder ID
        } else if (streamContextMapping[i].type == AVMEDIA_TYPE_AUDIO ) {
            encoderInfo = avcodec_find_encoder(audioCodecID);
        }           
        if(!encoderInfo){
            termination("Could not find encoder for stream.");
        }

        AVCodecContext *encoder = avcodec_alloc_context3(encoderInfo);                          //创建编码器上下文，Create encoder context
        if(!encoder){
            termination("Could not allocate encoder context.");
        }
   
        //设置编码器一些必要参数，由于单纯的转编码无法改这些参数，所以一般从解码器的信息中复制
        //Set necessary parameters of the encoder, since it is not possible to change these parameters by transcoding, the parameters usually copied from the decoder
        AVCodecContext *decoder = streamContextMapping[i].decoder;
        if (streamContextMapping[i].type == AVMEDIA_TYPE_VIDEO){
            encoder->height = decoder->height;
            encoder->width = decoder->width;
            encoder->framerate = decoder->framerate;
            encoder->sample_aspect_ratio = decoder->sample_aspect_ratio;
            if (encoderInfo->pix_fmts){
                encoder->pix_fmt = encoderInfo->pix_fmts[0];
            }else
                encoder->pix_fmt = decoder->pix_fmt;

            //可以设置与编码相关的参数，如gop、去除b帧、码率等
            //You can set and related parameters such as gop, removing b frames, and bitrate
            // encoder->gop_size = 40;
            // encoder->max_b_frames = 0;
            //encoder->bit_rate = 2000000;
        } else if (streamContextMapping[i].type == AVMEDIA_TYPE_AUDIO){
            encoder->sample_rate = decoder->sample_rate;
            av_channel_layout_copy(&encoder->ch_layout, &decoder->ch_layout);
            encoder->channels = av_get_channel_layout_nb_channels(encoder->channel_layout);
            if(encoderInfo->sample_fmts)
                encoder->sample_fmt = encoderInfo->sample_fmts[0];
            else
                encoder->sample_fmt = decoder->sample_fmt;
        }

        encoder->time_base = AV_TIME_BASE_Q;                                                    //固定TimeBase为1/1000000，能防止能多奇怪问题，Fixed TimeBase is 1/1000000，can prevent strange problems

        AVDictionary *optionsDict = NULL;
        //av_dict_set(&optionsDict, "threads", "2", 0);                                         //可设置编码器的一些参数，如处理线程数，Set some parameters of the encoder, such as the number of processing threads
        ret = avcodec_open2(encoder, encoderInfo, &optionsDict);
        if(ret<0){
            termination("Could not open encoder.");
        }

        encoder->time_base = AV_TIME_BASE_Q;                                                    //一些编码器会修改timebase，这里做一次覆盖设置，Some encoders modify timebase, do an override setting here
        streamContextMapping[i].encoder = encoder;
    }
}

void Step_Operation_TransCode(AVPacket *packet, AVFrame *frame){
    unsigned int inIndex = packet->stream_index;
    unsigned int outIndex = streamContextMapping[inIndex].outIndex;
    AVStream *inStream = inFileHandle->streams[inIndex];
    AVStream *outStream = outFileHandle->streams[outIndex];

    //根据编码器的timebase换算packet的相关时间戳
    //Converting the packet's associated timestamp from the encoder's timebase
    av_packet_rescale_ts(packet, inStream->time_base, streamContextMapping[inIndex].decoder->time_base);

    //STEP::将数据包发送到解码器（异步）
    //STEP::Send the data packet to the decoder for decode (async)
    ret = avcodec_send_packet(streamContextMapping[inIndex].decoder, packet);
    if(ret<0){
        termination("Could not decoding.");
    }
    av_packet_unref(packet);

    while(1){
        //STEP::尝试从解码器取出原始帧
        //STEP::Try to get the original frame
        ret = avcodec_receive_frame(streamContextMapping[inIndex].decoder, frame);
        if(ret == AVERROR(EAGAIN)){
            break;
        } else if (ret == AVERROR_EOF){
            streamContextMapping[inIndex].isDecodeEnd = true;
            break;
        } else if (ret < 0){
            termination("Could not receive decoding.");
        }

        //STEP::将原始帧发送到编码器进行编码（异步）
        //STEP::Send the original frame to the encoder for encode (async)
        ret = avcodec_send_frame(streamContextMapping[inIndex].encoder, frame);
        if(ret<0){
            termination("Could not encoding.");
        }
        av_frame_unref(frame);

        while(1){
            //STEP::尝试从编码器取出编码后的数据包
            //STEP::Try to get the encoded packet
            ret = avcodec_receive_packet(streamContextMapping[inIndex].encoder, packet);
            if (ret == AVERROR(EAGAIN)){
                break;
            } else if (ret == AVERROR_EOF){
                streamContextMapping[inIndex].isEncodeEnd = true;
                break;
            } else if (ret < 0) {
                termination("Could not receive encoding.");
            }

            //将轨道序号修改为对应的输出文件轨道序号
            //Change the track number to the corresponding output file track number.
            packet->stream_index = outIndex;

            //根据输出流的timebase换算packet的相关时间戳
            //Converting the packet's associated timestamp from the encoder's timebase
            av_packet_rescale_ts(packet, streamContextMapping[inIndex].encoder->time_base, outStream->time_base);

            //封装packet，并写入输出文件
            //Mux the packet and write to the output file
            ret = av_interleaved_write_frame(outFileHandle, packet);
            if (ret < 0) {
                termination("Could not mux packet.");   
            }
            av_packet_unref(packet);
        }
    }
}

void Step_Operation_End(AVPacket *packet, AVFrame *frame){
    for(unsigned int i = 0; i < streamContextLength; i++) {
        unsigned int inIndex = i;
        unsigned int outIndex = streamContextMapping[inIndex].outIndex;

        //STEP::清理解码器中的数据
        //STEP::Clean up the decoder
        if(!streamContextMapping[i].isDecodeEnd){
            //向解码器发送NULL，告诉解码器无新的数据包
            //Send NULL to the decoder to tell it there is no new package
            avcodec_send_packet(streamContextMapping[inIndex].decoder, NULL);

            while(1){
                ret = avcodec_receive_frame(streamContextMapping[inIndex].decoder, frame);
                if(ret == AVERROR(EAGAIN)){                                     //解码器还有未处理完的数据，Decoder indicates that it has not processed all the data
                    continue;
                } else if (ret == AVERROR_EOF){                                 //解码器处理所有数据的标识，Decoder indicates that it has processed all the data
                    streamContextMapping[i].isDecodeEnd = true;
                    break;
                } else if (ret < 0) {
                    termination("Could not receive decoding.");
                }

                //STEP::将原始帧发送到编码器进行编码（异步）
                //STEP::Send the original frame to the encoder for encode (async)
                ret = avcodec_send_frame(streamContextMapping[inIndex].encoder, frame);
                if(ret<0){
                    termination("Could not encoding.");
                }
                av_frame_unref(frame);
            }
        }

        //STEP::清理编码器的数据
        //STEP::Clean up the encoder
        if(!streamContextMapping[i].isEncodeEnd){
            //向编码器发送NULL，告诉编码器无新的帧数据
            //Send NULL to the encoder to tell it there is no new frame
            avcodec_send_frame(streamContextMapping[inIndex].encoder, NULL);

            while(1){
                ret = avcodec_receive_packet(streamContextMapping[inIndex].encoder, packet);
                if (ret == AVERROR(EAGAIN)){                                                    //编码器还有未处理完的数据，Encoder indicates that it has not processed all the data
                    continue;
                } else if (ret == AVERROR_EOF){                                                 //编码器处理所有数据的标识，Encoder indicates that it has processed all the data
                    streamContextMapping[i].isEncodeEnd = true;
                    break;
                } else if (ret < 0) {
                    termination("Could not receive encoding.");
                }

                //将轨道序号修改为对应的输出文件轨道序号
                //Change the track number to the corresponding output file track number.
                packet->stream_index = outIndex;

                av_packet_rescale_ts(packet, streamContextMapping[inIndex].encoder->time_base, outFileHandle->streams[outIndex]->time_base);

                //封装packet，并写入输出文件
                //Mux the packet and write to the output file
                ret = av_interleaved_write_frame(outFileHandle, packet);
                if (ret < 0) {
                    termination("Could not mux packet.");   
                }
                av_packet_unref(packet);
            }
        }
    }
}

void Step_Operation(){
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        termination("Could not allocate AVPacket.");   
    }
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        termination("Could not allocate AVFrame.");   
    }

    //STEP::av_read_frame会将源文件解封装，并将数据放到packet
    //数据包一般是按dts（解码时间戳）顺序排列的
    //STEP::av_read_frame unpacks the source file and puts the data into packet
    //The packets are generally in dts (decoding timestamp) order
    while (av_read_frame(inFileHandle, packet) >= 0) {

        //根据之前的关联关系，判断是否舍弃此packet
        //Determine whether to discard this packet based on previous associations
        if(streamContextMapping[packet->stream_index].outIndex < 0){
            av_packet_unref(packet);
            continue;
        }

        //需要转编码的轨道，need to transcoding
        if(streamContextMapping[packet->stream_index].decoder && streamContextMapping[packet->stream_index].encoder){
            //进入转编码流程
            //Enter the transcoding process
            Step_Operation_TransCode(packet, frame);
        } 
        //不需要转编码的轨道，no need to transcoding
        else {
            //转换timebase（时间基），一般不同的封装格式下，时间基是不一样的
            //Converts the timebase, which is generally different for different package formats
            AVStream *inStream = inFileHandle->streams[packet->stream_index];
            AVStream *outStream = outFileHandle->streams[streamContextMapping[packet->stream_index].outIndex];
            av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);

            //将轨道序号修改为对应的输出文件轨道序号
            //Change the track number to the corresponding output file track number.
            packet->stream_index = streamContextMapping[packet->stream_index].outIndex;

            //封装packet，并写入输出文件
            //Mux the packet and write to the output file
            ret = av_interleaved_write_frame(outFileHandle, packet);
            if (ret < 0) {
                termination("Could not mux packet.");   
            }

            av_packet_unref(packet);
        }
    }

    //文件读取完成，但是编解码器中的数据未必全部处理完毕
    //The reading of the file is complete, but the data in the decoder and encoder may not be completely processed
    Step_Operation_End(packet, frame);

    av_packet_free(&packet);
    av_frame_free(&frame);
}

void Step_CloseCodec(){
    //STEP::释放编码器、解码器
    //STEP::Free encoders and decoders
    for(int i=0;i<streamContextLength;i++){
        if(streamContextMapping[i].decoder){
            avcodec_free_context(&streamContextMapping[i].decoder);
            streamContextMapping[i].decoder = NULL;
        }
        if(streamContextMapping[i].encoder){
            avcodec_free_context(&streamContextMapping[i].encoder);
            streamContextMapping[i].encoder = NULL;
        }
    }
}

void Step_End(){
    //STEP::写入输出文件尾信息
    //Write output file tail information
    ret = av_write_trailer(outFileHandle);
    if(ret < 0) {
        termination("Could not write the stream trailer to out file.");   
    }

    //STEP::关闭输出文件，并销毁具柄
    //STEP::Close the output file，and destroy the handle
    ret = avio_closep(&outFileHandle->pb);
    if(ret < 0) {
        termination("Could not close out file.");   
    }
    avformat_free_context(outFileHandle);

    //STEP::关闭输入文件，并销毁具柄
    //STEP::Close the input file，and destroy the handle
    avformat_close_input(&inFileHandle);

    //STEP::释放关联表
    //STEP::Free the association table
    av_free(streamContextMapping);
}

int main(int argc, char *argv[]){
    //STEP::打开源文件并获取源文件信息
    //STEP::Open input file and get input file information
    Step_OpenInFile();

    //STEP::初始化解码器
    //STEP::Initialize the decoder
    Step_OpenDecoder();
    
    //STEP::初始化编码器
    //STEP::Initialize the encoder
    Step_OpenEncoder();

    //STEP::构造输出文件
    //STEP::Constructing output files
    Step_CreateOutFile();

    //STEP::循环处理数据
    //STEP::Cyclic processing data
    Step_Operation();

    //STEP::关闭编码器、解码器
    //STEP::Close encoder and decoder
    Step_CloseCodec();

    //STEP::关闭输入、输出文件
    //STEP::Close input and output files
    Step_End();
}