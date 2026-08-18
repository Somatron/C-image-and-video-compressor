#define STB_IMAGE_IMPLEMENTATION
#include "jpeg-compressor-master/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "jpeg-compressor-master/stb_image_write.h"

#include <iostream>
#include <fstream>
#include <string>

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

