extern "C" { //import C functions since C++ supports function overloading and C does not
  //include codec formatt and util opt.h
#include <ffmpeg-9.0.1/libavcodec/avcodec.h>
#include <ffmpeg-9.0.1/libavformat/avformat.h>
#include <ffmpeg-9.0.1/libavutil/imgutils.h>
#include <ffmpeg-9.0.1/libavutil/opt.h>
}
#include <iostream>


#include "httplib.h" //add the html to run c++

void video_compress(const char* input_file, const char* output_file) {
  //format video
  //AVFormat is a struct btw, structs group variables together, AVFormat handles pointers for input/output, filenames, streams (audio, subtitles, etc), and bit rates
  AVFormatContext* inputFormatContext = nullptr; //point to nothing
  AVFormatContext* outputFormatContext = nullptr;

  //open input video file, with inputFormatcontext, and input_file being our URL
  avformat_open_input(&inputFormatContext, input_file, nullptr, nullptr); //what happens to our inputFormatContext in the avformat_open_input function impacts our variable here too

  //& kills local scopes
  avformat_find_stream_info(inputFormatContext, nullptr); //we dont need AVDictionary **options for the 2nd arguement

  int videoStreamIndex = -1;
  for (unsigned int i = 0; i < inputFormatContext -> nb_streams; i++) { //unsigned means only positive ints, loop thru all streams
    if (inputFormatContext -> streams[i] -> codecpar -> codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStreamIndex = i; //check if media type is a video, which saves current index into the videostream, found the video track needed
      break;       
    }
  }

  if (videoStreamIndex == -1) {
    return; //if we still have -1 then we probably didnt find the stream we were looking for
  }

  //extract pointers of video stream from our input
  AVStream *input_stream = inputFormatContext -> streams[videoStreamIndex];
  const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) {
    std::cerr << "ERROR encoder is not found" << std::endl; 
  }
  AVStream *output_stream = avformat_new_stream(outputFormatContext, codec); //output our new video stream under the H.264 format since its a better more universally used compression method than H.265
  AVCodecContext *codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    std::cerr << "ERROR cannot allocate memory for encoding/decoding media streams" << std::endl; 
  }

  //begin compression
  codec_context -> width = input_stream->codecpar->width;
  codec_context -> height = input_stream->codecpar->height;
  codec_context -> time_base = input_stream->time_base; //video timestamp
  codec_context -> pixel_format = AV_PIX_FMT_YUV420P; //very compressible 
  codec_context -> bitrate = 1000000; //lower target bitrate

  //set encoding speed/compression trade-off profile
  av_opt_set(codec_context->priv_data, "preset", "slow", 0);
  if (avcodec_open2(codec_context, codec, NULL) < 0) {
    std::cerr << "ERROR cannot open video format" << std::endl; 
    return;
  }

  avcodec_parameters_from_context(output_stream->codecpar, codec_context);

  //open output file mapping and write container headers
  if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
    avio_open(&outputFormatContext->pb, output_file, AVIO_FLAG_WRITE); //point to pb, our file url path, flags
  }
  avformat_write_header(outputFormatContext, nullptr); //write header file

  std::cout << "MP4 Video has been compressed successfully" << endl;

  //free memory ofc
  avcodec_free_context(&codec_context);
  avformat_close_input(&inputFormatContext);
  if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&outputFormatContext->pb);
  }
  avformat_free_context(outputFormatContext); 

}