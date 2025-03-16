# RTP H264 packetizer

Converts raw H264 units to RTP packets.

## Build Sample

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

make
```

## Sample Usage

The sample streams RTP packets over UDP on port 6000.

```
ffplay  -i sample/h264.sdp -protocol_whitelist rtp,file,udp
```