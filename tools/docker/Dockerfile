# Build environment for the Endless Sky 3DS port: devkitARM, plus what the
# asset converter needs (Python with Pillow and numpy, a host C++ compiler).
FROM devkitpro/devkitarm:latest

RUN apt-get update \
	&& apt-get install -y --no-install-recommends python3-pil python3-numpy g++ \
	&& rm -rf /var/lib/apt/lists/*
