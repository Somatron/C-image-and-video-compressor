# How to use:

## To use run: 
#### 1. docker build -t image-video-compressor .

#### 2. docker run -d -p 8080:8080 --name compressor-app image-video-compressor

# Credits:

Huge Huge Huge thanks to this video for explaining Video Compression step by step, this explains the process of frame encoding/decoding, video compression techniques, and so much more: https://www.youtube.com/watch?v=LDeL7-49qm4 

Check out https://github.com/ffmpeg/ffmpeg I used this library to create audio and video compression, shoutout Fabrice Bellard for creating the whole internet video infrastructure, a lot of this code had me look up a lot of functions and docs on libavformat and libavcodec but i finally finished this at last
Also used Crow https://crowcpp.org/master/ to run C++ code on HTML that was fun and waaaay better than using http::lib originally

I also used https://github.com/richgel999/jpeg-compressor for compressing image files into jpeg


Most of the project works, except for the video being delayed and slow for the audio lol, heres how the image compression looks like:

!# Original: <img width="447" height="447" alt="images" src="https://github.com/user-attachments/assets/49d35fe3-72e0-406f-973e-3ff2ce1ad13d" />!# Compressed:<img width="447" height="447" alt="compressed_image (2)" src="https://github.com/user-attachments/assets/090ec3cc-02fd-4900-8eac-a9691967b205" />

feel free to use this or whatevr you want to do with it
