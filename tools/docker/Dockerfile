# Build environment for the Endless Sky 3DS port: devkitARM, plus what the
# asset converter needs (Python with Pillow and numpy, a host C++ compiler),
# plus makerom and bannertool to build the CIA.
FROM devkitpro/devkitarm:latest

RUN apt-get update \
	&& apt-get install -y --no-install-recommends python3-pil python3-numpy g++ make git ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

# makerom (Project_CTR) and bannertool, built from source at known commits.
ARG PROJECT_CTR_COMMIT=e8f5f529c54ff9b22a2491a480ffa69206bf7b19
ARG BANNERTOOL_COMMIT=16d8c5a0ce02a5e06e64ab42275132fca57c04a2
RUN git clone https://github.com/3DSGuy/Project_CTR.git /tmp/ctr \
	&& git -C /tmp/ctr checkout -q "$PROJECT_CTR_COMMIT" \
	&& make -C /tmp/ctr/makerom deps -j"$(nproc)" \
	&& make -C /tmp/ctr/makerom -j"$(nproc)" \
	&& install -m 755 /tmp/ctr/makerom/bin/makerom /usr/local/bin/makerom \
	&& git clone https://github.com/diasurgical/bannertool.git /tmp/bannertool \
	&& git -C /tmp/bannertool checkout -q "$BANNERTOOL_COMMIT" \
	&& cd /tmp/bannertool \
	&& for f in $(find source -name '*.c'); do gcc -O2 -Isource -c "$f" -o "${f%.c}.o"; done \
	&& g++ -O2 -std=c++11 -DVERSION_MAJOR=1 -DVERSION_MINOR=2 -DVERSION_MICRO=0 -Isource \
		$(find source -name '*.cpp') $(find source -name '*.o') -o /usr/local/bin/bannertool \
	&& rm -rf /tmp/ctr /tmp/bannertool
