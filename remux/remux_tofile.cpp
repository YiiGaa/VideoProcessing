/*
 * 视频转封装例子，适合文件转文件、直播流转文件、直播流转直播流
 * The sample of remuxing video, Suitable for file to file, live streaming to file, live streaming to live streaming
 * Depends on FFmpeg 6.0
 * Wirte by stoprefactoring.com
*/

#include <iostream>
extern "C" {  
    #include <libavutil/timestamp.h>
    #include <libavformat/avformat.h>
}

int ret = 0;

//输入输出文件路径
//Input and output file paths
const char *inFilePath  = "../../common/test.mp4";
//const char *inFilePath  = "rtmp://192.168.3.202:1935/live/test";
const char *outFilePath  = "./out.flv";

//输入输出文件句柄
//Input and output file handles
AVFormatContext *inFileHandle = NULL;
AVFormatContext *outFileHandle = NULL;

//输入文件、输出文件的轨道序号关联表
//Track number correlation table for input files and output files
int *streamMapping = NULL;

void termination(const char* param){
    std::cout<<param<<std::endl;
    std::cout<<"Error occur, quit!"<<std::endl;
    exit(-1);
}

void Step1_OpenInFile(){
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
}

void Step2_CreateOutFile(){
    //STEP::创建输出文件句柄outFileHandle
    //STEP::Creates an output file handle, outFileHandle.
    ret = avformat_alloc_output_context2(&outFileHandle, NULL, "flv", outFilePath);
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
    int outStreamIndex = 0;
    streamMapping = (int *)av_malloc_array(inFileHandle->nb_streams, sizeof(*streamMapping));
    if(!streamMapping){
        termination("Could not allocate stream mapping.");
    }
    for(unsigned int i = 0; i < inFileHandle->nb_streams; i++) {
        AVStream *inStream = inFileHandle->streams[i];
        if (inStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&                               //过滤除video、audio、subtitle以外的轨道，Filter tracks except video, audio, subtitle
            inStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            inStream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            streamMapping[i] = -1;
            continue;
        }

        AVStream *outStream = avformat_new_stream(outFileHandle, NULL);                            //创建输出的轨道，Creating the output track
        ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);                    //复制源轨道的信息到输出轨道，Copying information from the source track to the output track
        if(ret<0){
            termination("Could not copy codec parameters.");
        }
        outStream->codecpar->codec_tag = 0;
        streamMapping[i] = outStreamIndex++;                                                       //记录源文件轨道序号与输出文件轨道序号的对应关系，Record the correspondence between the track number of the source file and the track number of the output file.
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

void Step3_Operation(){
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        termination("Could not allocate AVPacket.");   
    }

    //STEP::av_read_frame会将源文件解封装，并将数据放到packet
    //数据包一般是按dts（解码时间戳）顺序排列的
    //STEP::av_read_frame unpacks the source file and puts the data into packet
    //The packets are generally in dts (decoding timestamp) order
    while (av_read_frame(inFileHandle, packet) >= 0) {

        //根据之前的关联关系，判断是否舍弃此packet
        //Determine whether to discard this packet based on previous associations
        if(streamMapping[packet->stream_index] < 0){
            av_packet_unref(packet);
            continue;
        }

        //转换timebase（时间基），一般不同的封装格式下，时间基是不一样的
        //Converts the timebase, which is generally different for different package formats
        AVStream *inStream = inFileHandle->streams[packet->stream_index];
        AVStream *outStream = outFileHandle->streams[streamMapping[packet->stream_index]];
        av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);

        //将轨道序号修改为对应的输出文件轨道序号
        //Change the track number to the corresponding output file track number.
        packet->stream_index = streamMapping[packet->stream_index];

        //封装packet，并写入输出文件
        //Mux the packet and write to the output file
        ret = av_interleaved_write_frame(outFileHandle, packet);
        if (ret < 0) {
            termination("Could not mux packet.");   
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
}

void Step4_End(){
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
}

int main(int argc, char *argv[]){
    //STEP::打开源文件并获取源文件信息
    //STEP::Open input file and get input file information
    Step1_OpenInFile();

    //STEP::构造输出文件
    //STEP::Constructing output files
    Step2_CreateOutFile();

    //STEP::循环处理数据
    //STEP::Cyclic processing data
    Step3_Operation();

    //STEP::关闭输入、输出文件
    //STEP::Close input and output files
    Step4_End();
}