# Slowmovie

Play one frame per second of a movie on a ePaper display. This is a wild way to watch a movie since it takes months to get through what would normally be a few hours. The display becomes a showpiece as you wonder what scenes are being shown whenever you pass it by.

This installation uses an ESP32 and a 2.7" ePaper display. Images are passed via MQTT by a server-size script that uses ffmpeg to grab the image and imagemagick to format it.

## Crontab

Run this script every minute using the following crontab job:
`* * * * * cd /home/mike/compile/slowmovie && /usr/bin/python3 -c "from slowmovie_framepublisher import processNextFrame; processNextFrame()"`

