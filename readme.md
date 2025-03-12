-DCMAKE_BUILD_TYPE=Debug


ffplay  -i h264.sdp -protocol_whitelist rtp,file,udp


ffmpeg -re -i test.264 -c:v libx264 -preset ultrafast -f rtp rtp://127.0.0.1:6000
ffmpeg -re -i Big_Buck_Bunny_1080_10s_5MB.mp4 -c:v libx264 -preset ultrafast -f rtp rtp://127.0.0.1:6000