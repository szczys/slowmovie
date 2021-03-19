# Slowmovie

Play one frame per second of a movie on a ePaper display. This is a wild way to watch a movie since it takes months to get through what would normally be a few hours. The display becomes a showpiece as you wonder what scenes are being shown whenever you pass it by.

This installation uses an ESP32 and a 2.7" ePaper display. Images are passed via MQTT by a server-size script that uses ffmpeg to grab the image and imagemagick to format it.

## Notes on dev583 branch

In development for 5.83" ePaper screen

Oneliner for formatting the frames:
`convert frame.png -resize "648x480^" -gravity center -crop 648x480+0+0 -dither FloydSteinberg frame.png`

Oneliner for pushing a frame to MQTT:
`mosquitto_pub -h 192.168.1.135 -t "slowmovie/frame" -f frame.pbm`

## Crontab

Run this script every minute using the following crontab job:
`* * * * * cd /home/mike/compile/slowmovie && /usr/bin/python3 -c "from slowmovie_framepublisher import processNextFrame; processNextFrame()"`

