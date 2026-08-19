//GRAB VIDEO LIBRARIES
extern "C" { //import C functions since C++ supports function overloading and C does not
  //include codec formatt and util opt.h
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/imgutils.h>
  #include <libavutil/opt.h>
}

//GRAB IMAGE LIBRARIES 
#define STB_IMAGE_IMPLEMENTATION
#include "jpeg-compressor-master/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "jpeg-compressor-master/stb_image_write.h"

#include <iostream>
#include <fstream>
#include <string>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "crow.h" //crow is a flask inspired framework to run c++ on html

//compress input image by adjusting quality
bool Img_to_JPEG(const std::string& inputPath, const std::string& outputPath, int img_quality) {
  int width, height, channels; 

  //load raw data from file
  unsigned char* imgData = stbi_load(inputPath.c_str(), &width, &height, &channels, 0); //# of channels we want output to have
  if (!imgData) { //img fails to be loaded
    std::cerr << "Failed to load image: " << inputPath << std::endl;
    return false; //cerr is error message
  }

  //open file safely
  FILE* open_file = fopen(outputPath.c_str(), "wb");
  if (!open_file) { //error handling
    std::cerr << "Failed to open file: " << outputPath << std::endl;
    stbi_image_free(imgData); //free imgData from memory 
    return false;
  }

  //set params for the writer
  //since creating a jpg takes in multiple arguements, going to be stbi_write_func *func, void *context, int x, int y, int comp, const void *data, int quality

  int jpg_created = stbi_write_jpg_to_func([](void* context, void* data, int size) { //lambda function from w3 schools
      FILE* file_jpeg = static_cast<FILE*>(context); //cast into input file yawn
      fwrite(data, 1, size, file_jpeg); //lowest quality with highest compression
  }, open_file, width, height, channels, imgData, img_quality);

  fclose(open_file); //dont forget to close
  stbi_image_free(imgData); //and free imgData from memory

  if (!jpg_created) {
    std::cerr << "Failed to create Jpeg image DAMN IT!: " << std::endl;
    return false;
  }
  
  return true;
}



void video_compress(const char* input_file, const char* output_file) {
  //format video
  //AVFormat is a struct btw, structs group variables together, AVFormat handles pointers for input/output, filenames, streams (audio, subtitles, etc), and bit rates
  AVFormatContext* inputFormatContext = nullptr; //point to nothing
  AVFormatContext* outputFormatContext = nullptr;
  AVCodecContext* encoder_context = nullptr;
  AVCodecContext* decoder_context = nullptr;

  //1. open input video file, with inputFormatcontext, and input_file being our URL
  //what happens to our inputFormatContext in the avformat_open_input function impacts our variable here too
    if (avformat_open_input(&inputFormatContext, input_file, nullptr, nullptr) < 0) {
      return; //make sure function works
    }
    //& kills local scopes
    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {//we dont need AVDictionary **options for the 2nd arguement
      return;
    }

  //2. Find the video stream
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < inputFormatContext -> nb_streams; i++) { //unsigned means only positive ints, loop thru all streams
      if (inputFormatContext -> streams[i] -> codecpar -> codec_type == AVMEDIA_TYPE_VIDEO) {
        videoStreamIndex = i; //check if media type is a video, which saves current index into the videostream, found the video track needed
        break;       
      }
    }
    if (videoStreamIndex == -1) {
      avformat_close_input(&inputFormatContext); //close memory
      return; //if we still have -1 then we probably didnt find the stream we were looking for
    }
    //extract pointers of video stream from our input
    AVStream *input_stream = inputFormatContext -> streams[videoStreamIndex];


  //3. decoder set up
    const AVCodec *decoder = avcodec_find_decoder(input_stream->codecpar->codec_id);
    if (!decoder) {
      std::cerr << "ERROR decoder not found" << std::endl;
      avformat_close_input(&inputFormatContext);
      return;
    }
    decoder_context = avcodec_alloc_context3(decoder); //create memory to decode first
    avcodec_parameters_to_context(decoder_context, input_stream->codecpar);
    if (avcodec_open2(decoder_context, decoder, nullptr) < 0) {
      avcodec_free_context(&decoder_context); //free memory if we fail
      avformat_close_input(&inputFormatContext);
      return;
    }


  //4. output container and we will use H.264 Encoder for our compression

    avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, output_file); //allocate the outputFormatContext

    int audioStreamIndex = -1;
    int outputAudioStreamIndex = -1;
    //start finding our audio streams
    for (unsigned int i = 0; i < inputFormatContext -> nb_streams; i++) {
      if (inputFormatContext -> streams[i] -> codecpar -> codec_type == AVMEDIA_TYPE_AUDIO) { //basically the same for finding our video streams except its audio actually
        audioStreamIndex = i;
        break;
      }
    }

    if (!outputFormatContext) {
      avcodec_free_context(&decoder_context); //fail again
      avformat_close_input(&inputFormatContext);
      return;
    }

    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder) {
      std::cerr << "ERROR encoder not found" << std::endl; 
      avformat_close_input(&inputFormatContext); //close memory
      return;
    }
    AVStream *output_video_stream = avformat_new_stream(outputFormatContext, encoder); //output our new video stream under the H.264 format since its a better more universally used compression method than H.265
    encoder_context = avcodec_alloc_context3(encoder);
    if (!encoder_context) {
      std::cerr << "ERROR cannot allocate memory for encoding/decoding media streams" << std::endl; 
      return;
    }

    //begin compression
    encoder_context -> width = decoder_context->width;
    encoder_context -> height = decoder_context->height;
    encoder_context -> time_base = (AVRational){1, 25}; //frame rate basically
    encoder_context -> pix_fmt = AV_PIX_FMT_YUV420P; //very compressible 
    encoder_context -> bit_rate = 400000; //lower target bitrate
    output_video_stream->time_base = encoder_context->time_base;

    //crf = constant rate factor, adjust bitrate to based on visual quality instead of fixed to size of the file
    av_opt_set(encoder_context->priv_data, "crf", "28", 0); //28 is our quality value
    //set encoding speed/compression trade-off profile
    av_opt_set(encoder_context->priv_data, "preset", "slow", 0);
    if (avcodec_open2(encoder_context, encoder, nullptr) < 0) {
      std::cerr << "ERROR cannot open video format" << std::endl; 
      avcodec_free_context(&encoder_context); //Free memory of encoder and decoder
      avcodec_free_context(&decoder_context);
      avformat_close_input(&inputFormatContext); //close context for inputs and outputs
      avformat_free_context(outputFormatContext);
      return;
    }
  
    avcodec_parameters_from_context(output_video_stream->codecpar, encoder_context);

  
    //Pass the original audio track into our new compressed file
    if (audioStreamIndex != -1) {
      AVStream *in_audio_stream = inputFormatContext -> streams[audioStreamIndex];
      AVStream *out_audio_stream = avformat_new_stream(outputFormatContext, nullptr);
      //parameters(target, to copy from)
      avcodec_parameters_copy(out_audio_stream -> codecpar, in_audio_stream -> codecpar); //a lot of searching online to try to figure out how to move audio from input to compressed file
      out_audio_stream -> codecpar -> codec_tag = 0;
      outputAudioStreamIndex = out_audio_stream -> index; //point to audio track, where it is stored
    }


    //open output file mapping and write container headers
    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
      if (avio_open(&outputFormatContext->pb, output_file, AVIO_FLAG_WRITE) < 0) { 
        avcodec_free_context(&encoder_context); //Free memory of encoder and decoder
        avcodec_free_context(&decoder_context);
        avformat_close_input(&inputFormatContext); //close context for inputs and outputs
        avformat_free_context(outputFormatContext);
        return;
      } //point to pb, our file url path, flags
    }

    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
          avcodec_free_context(&encoder_context); //Free memory of encoder and decoder
          avcodec_free_context(&decoder_context);
          avformat_close_input(&inputFormatContext); //close context for inputs and outputs
          avformat_free_context(outputFormatContext);
          return;
    }



  //5. Time loop
    AVPacket* in_packet = av_packet_alloc(); //create memory for input frame, output compressed frame, and overall frame
    AVFrame* frame = av_frame_alloc();
    AVPacket* out_packet = av_packet_alloc();
    int64_t pts_counter = 0;
    
    while (av_read_frame(inputFormatContext, in_packet) >= 0) { //count frame by frame
      if (in_packet->stream_index == videoStreamIndex) { //see if our input packets match the video
        if (avcodec_send_packet(decoder_context, in_packet) >= 0) {
          while (avcodec_receive_frame(decoder_context, frame) >= 0) { //send packets to the decoder and have them recieve frame

            frame->pts = pts_counter++; //Prepare frame timestamps for encoding

            //send decoded frame to encoder
            if (avcodec_send_frame(encoder_context, frame) >= 0) {
              while (avcodec_receive_packet(encoder_context, out_packet) >= 0) {

                av_packet_rescale_ts(out_packet, encoder_context->time_base, output_video_stream->time_base);
                out_packet->stream_index = output_video_stream->index; //whenever we have multiple frames the best thing we should do is cut redundencies between each fames, this is where we have different data packets for compressing medias, we move packets from where they need to be compressed

                //write compressed video packet to output stream
                av_interleaved_write_frame(outputFormatContext, out_packet);
                av_packet_unref(out_packet); //free packet after we write it

              }
            }
          }
        }
      } else if (in_packet -> stream_index == audioStreamIndex && outputAudioStreamIndex != -1) {
          AVStream *in_audio_stream = inputFormatContext -> streams[audioStreamIndex]; //copying Audio Packets into output
          AVStream *out_audio_stream = outputFormatContext -> streams[outputAudioStreamIndex];

          //rescaling timestamps for the audio stream, well first we need to make sure they're valid first

          //dts tells us when to decode video frame
          //pts tells us when to display the decoded frame on screen
          in_packet -> pts = av_rescale_q_rnd(in_packet -> pts, in_audio_stream->time_base, out_audio_stream -> time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
          in_packet -> dts = av_rescale_q_rnd(in_packet -> dts, in_audio_stream->time_base, out_audio_stream -> time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
          in_packet -> duration = av_rescale_q(in_packet -> duration, in_audio_stream->time_base, out_audio_stream -> time_base); //now we rescale
          in_packet -> pos = -1;
          in_packet -> stream_index = outputAudioStreamIndex; //audio packets transition from our original file to compressed file

          av_interleaved_write_frame(outputFormatContext, in_packet); //write audio frame
      }                        
      av_packet_unref(in_packet); //free input packet
    }



  //6. Force any data encoder to process and output any remaining frames for compression
    avcodec_send_frame(encoder_context, nullptr);
    while (avcodec_receive_packet(encoder_context, out_packet) >= 0) {
      av_packet_rescale_ts(out_packet, encoder_context->time_base, output_video_stream->time_base);
      out_packet->stream_index = output_video_stream->index; 
      
      av_interleaved_write_frame(outputFormatContext, out_packet);
      av_packet_unref(out_packet);
    }

    //Write the stream trailer to an output media file and free the file private data.
    av_write_trailer(outputFormatContext);
    std::cout << "MP4 Video has been compressed successfully" << std::endl;


  //7. Free memory
    av_packet_free(&in_packet);
    av_frame_free(&frame);
    av_packet_free(&out_packet);

    avcodec_free_context(&encoder_context); //Free memory for encoders and decoders
    avcodec_free_context(&decoder_context);
    avformat_close_input(&inputFormatContext);
    if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
      avio_closep(&outputFormatContext->pb);
    }
    avformat_free_context(outputFormatContext); 
}

int main() {
  crow::SimpleApp app;

  CROW_ROUTE(app, "/")([]() {
    crow::response res;
    std::ifstream file("web/img-vid-engine.html");
    
    if(file.is_open()) {
      file.open("img-vid-engine.html");
    }

    if (!file.is_open()) {
      res.code = 500;
      res.body = "Error: HTML interface template missing.";
      return res;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    res.set_header("Content-Type", "text/html");
    res.body = content;
    return res;
  });

  CROW_ROUTE(app, "/compress-image").methods("POST"_method)([](const crow::request& req) {
    crow::multipart::message file_message(req);
    auto part = file_message.get_part_by_name("file");

    if (part.body.empty()) {
      return crow::response(400, "Missing 'file' field inside muiltipart data.");
    }
    
    //save uploaded image
    std::ofstream out("uploaded_img", std::ios::binary);
    out.write(part.body.c_str(), part.body.size());
    out.close();

    //compression time
    if (Img_to_JPEG("uploaded_img", "result_img.jpg", 50)) {
      std::ifstream in("result_img.jpg", std::ios::binary);
      std::string compressed_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

      crow::response res;
      res.code = 200;
      res.set_header("Content-Type", "image/jpeg");
      res.body = compressed_data;
      return res;
    }
    else {
      return crow::response(500, "Image compression failed.");
    }

  });

  CROW_ROUTE(app, "/compress-video").methods("POST"_method)([](const crow::request& req) {
    crow::multipart::message file_message(req);
    auto part = file_message.get_part_by_name("file"); //get them from the form

    if (part.body.empty()) {
      return crow::response(400, "Missing 'file' field inside multipart data.");
    }

    //save video
    std::ofstream out("uploaded_video.mp4", std::ios::binary);
    out.write(part.body.c_str(), part.body.size());
    out.close();

    video_compress("uploaded_video.mp4", "result_vid_compressed.mp4");
    std::ifstream in("result_vid_compressed.mp4", std::ios::binary);
    std::string compressed_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    crow::response res;
    res.code = 200;
    res.set_header("Content-Type", "video/mp4");
    res.body = compressed_data;
    return res;
    
  });

  std::cout << "Server is now running on http://localhost:8080" << std::endl;
  app.port(8080).multithreaded().run();
}
