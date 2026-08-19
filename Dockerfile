# Step 1: Use an official Ubuntu Linux base image with compilation tools
FROM ubuntu:22.04 AS build

# Prevent interactive prompts during installation phase
ENV DEBIAN_FRONTEND=noninteractive

# Install essential compilers, networking libraries, and the FFmpeg developer packages
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libasio-dev \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Download Crow Single Header
RUN mkdir -p /usr/local/include && \
    curl -L https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h -o /usr/local/include/crow.h

# Download stb image headers for JPEG compression
RUN mkdir -p /tmp/stb && \
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o /tmp/stb/stb_image.h && \
    curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -o /tmp/stb/stb_image_write.h

# Set the working environment directory inside the container
WORKDIR /app

# Copy your local mainEngine.cpp and dependencies into the active container context
COPY mainEngine.cpp .
RUN mkdir -p jpeg-compressor-master && \
    cp /tmp/stb/stb_image.h jpeg-compressor-master/ && \
    cp /tmp/stb/stb_image_write.h jpeg-compressor-master/

# Compile targeting Crow and FFmpeg
RUN g++ -std=c++17 mainEngine.cpp -o compressor \
    -lavcodec -lavformat -lavutil -lpthread

# Step 2: Use a clean runtime layer to keep the final image incredibly lightweight
FROM ubuntu:22.04

# Prevent interactive prompts during installation phase
ENV DEBIAN_FRONTEND=noninteractive

# Install the correct runtime library versions mapped to Ubuntu 22.04
RUN apt-get update && apt-get install -y \
    libavcodec58 \
    libavformat58 \
    libavutil56 \
    libswscale5 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Pull only the compiled executable binary from the build layer
COPY --from=build /app/compressor .

# Copy your frontend static assets folder into the engine context
COPY web/ ./web/

# Expose the network port your cpp-httplib server listens on (e.g., 8080)
EXPOSE 8080

# Spin up your video compressing application when the container starts
CMD ["./compressor"]
