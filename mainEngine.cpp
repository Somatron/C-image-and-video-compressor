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

#include "httplib.h" //add the html to run c++

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

  std::cout << "COMPRESSED IMAGE, SAVED INTO: " << outputPath << "\nQuality: " << img_quality << "%\n";
  return true;
}



void video_compress(const char* input_file, const char* output_file) {
  //format video
  //AVFormat is a struct btw, structs group variables together, AVFormat handles pointers for input/output, filenames, streams (audio, subtitles, etc), and bit rates
  AVFormatContext* inputFormatContext = nullptr; //point to nothing
  AVFormatContext* outputFormatContext = nullptr;

  //open input video file, with inputFormatcontext, and input_file being our URL
  avformat_open_input(&inputFormatContext, input_file, nullptr, nullptr); //what happens to our inputFormatContext in the avformat_open_input function impacts our variable here too
  if (avformat_open_input(&inputFormatContext, input_file, nullptr, nullptr) < 0) {
    return; //make sure function works
  }


  //& kills local scopes
  avformat_find_stream_info(inputFormatContext, nullptr); //we dont need AVDictionary **options for the 2nd arguement
  if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
    return;
  }

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
  const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) {
    std::cerr << "ERROR encoder is not found" << std::endl; 
    avformat_close_input(&inputFormatContext); //close memory
    return;
  }

  avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, output_file); //allocate the outputFormatContext
  if (!outputFormatContext) {
    return;
  }

  AVStream *output_stream = avformat_new_stream(outputFormatContext, codec); //output our new video stream under the H.264 format since its a better more universally used compression method than H.265
  AVCodecContext *codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    std::cerr << "ERROR cannot allocate memory for encoding/decoding media streams" << std::endl; 
    return;
  }

  //begin compression
  codec_context -> width = input_stream->codecpar->width;
  codec_context -> height = input_stream->codecpar->height;
  codec_context -> time_base = input_stream->time_base; //video timestamp
  codec_context -> pix_fmt = AV_PIX_FMT_YUV420P; //very compressible 
  codec_context -> bit_rate = 1000000; //lower target bitrate

  //set encoding speed/compression trade-off profile
  av_opt_set(codec_context->priv_data, "preset", "slow", 0);
  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    std::cerr << "ERROR cannot open video format" << std::endl; 
    return;
  }

  avcodec_parameters_from_context(output_stream->codecpar, codec_context);

  //open output file mapping and write container headers
  if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
    avio_open(&outputFormatContext->pb, output_file, AVIO_FLAG_WRITE); //point to pb, our file url path, flags
  }
  if (avformat_write_header(outputFormatContext, nullptr) < 0) {
    std::cerr << "Error writing format header" << std::endl;
  }

  //Write the stream trailer to an output media file and free the file private data.
  av_write_trailer(outputFormatContext);

  std::cout << "MP4 Video has been compressed successfully" << std::endl;

  //free memory ofc
  avcodec_free_context(&codec_context);
  avformat_close_input(&inputFormatContext);
  if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&outputFormatContext->pb);
  }
  avformat_free_context(outputFormatContext); 
}

int main() {
  httplib::Server eng_server;

  eng_server.Get("/", [](const httplib::Request &req, httplib::Response &res){
    std::ifstream file("img-vid-engine.html");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    res.set_content(content, "text/html"); //use text/html to support images, gifs, videos, etc.
  });
  
  eng_server.Post("/compress-image", [](const httplib::Request &req, httplib::Response &res){
    // For multipart form data with cpp-httplib
    if (req.has_param("file")) {
      std::string file_data = req.get_param_value("file");
      std::ofstream out("uploaded_temp_img", std::ios::binary);
      out.write(file_data.c_str(), file_data.size());
      out.close();

      Img_to_JPEG("uploaded_temp_img", "compressed_output.jpg", 50);

      std::ifstream in("compressed_output.jpg", std::ios::binary);
      std::string compressed_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      res.set_content(compressed_data, "image/jpeg");
    }
    else {
      res.status = 400;
      res.set_content("No file uploaded under key 'file'", "text/html");
    }
  });

  std::cout << "Server is now running on http://localhost:8080" << std::endl;
  eng_server.listen("0.0.0.0", 8080);
  return 0;
}
