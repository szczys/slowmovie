# Slowmovie

Play one frame per second of a movie on a ePaper display. This is a wild way to
watch a movie since it takes months to get through what would normally be a few
hours. The display becomes a showpiece as you wonder what scenes are being
shown whenever you pass it by.

In practice I advance the frame counter by 5 each time the script runs, and I
run the script using a cron job every four minutes. So if you consider 24-ish
frames per second, the movie advances about 1 second every 20 minutes.

This installation uses an ESP32 and an ePaper display. Images are passed via
MQTT by a server-size script that uses FFmpeg to grab the image and ImageMagick
to format it. The script is capable of producing images for several different
screen sizes if you want to feed a few screens the same image.

## Installation

Linux dependencies:

```
sudo apt install imagemagick ffmpeg
```

Install the python dependencies:

```
pip install -r requirements.txt
```

### Serving the images

This application depends on image being publicly available on a server. The
server address and path/filename are specified in the firmware source code
using the `HOST_NAME` and `PATH_NAME` defines. I symlink the generated image
files to the `/var/www/html` directory of my Apache2 server to make these files
available on my LAN.

### Publishing the updates

Device are notified when new frames are available by publishing to an MQTT
broker. The same broker must be used for the image publishing python script
(configured below) and the firmware (located in subdirectories of this
project).

## Configuration

Adjust your configuration in the `slowmovie-config.yml` file.

```yaml
movie:
  video_file: "/home/mike/compile/slowmovie/input.mkv"
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

Run the frame publishing script every 4 minutes using the following crontab
job:

```
*/4 * * * * bash -lc /home/mike/compile/slowmovie/slowmovie_framepublisher.py
```
