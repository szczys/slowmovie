# Slowmovie

Play one frame per second of a movie on a ePaper display. This is a wild way to watch a movie since it takes months to get through what would normally be a few hours. The display becomes a showpiece as you wonder what scenes are being shown whenever you pass it by.

This installation uses an ESP32 and a 2.7" ePaper display. Images are passed via MQTT by a server-size script that uses ffmpeg to grab the image and imagemagick to format it.

## Installation

Install the python dependencies:

```
pip install -r requirements.txt
```

## Configuration

Adjust your configuration in the `slowmovie-config.yml` file.

```yaml
movie:
  video_file: "input.mkv"
  prefix: "frame"   # Optional
  frame_divisor: 5  # Optional

mqtt:
  addr: "192.168.1.135"
  topic: "slowmovie/frame"

screen_sizes:
  # At least one screen size is required
  - name: "640x384"
    x: 640
    y: 384
  - name: "800x480"
    x: 800
    y: 480
```

## Crontab

Run this script every minute using the following crontab job:

```
* * * * * /home/mike/compile/slowmovie/slowmovie_framepublisher.py
```

