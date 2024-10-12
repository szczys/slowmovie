#!/usr/bin/env python

"""
slowmovie


MIT License

Copyright (c) 2020-2024 Mike Szczys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""

import subprocess
import json
import os
import paho.mqtt.client as mqtt
import datetime
import yaml

class SourceVideo:
    def __init__(self, video_file: str, prefix: str, working_dir: str):
        self.video_file = video_file
        self.total_frames = self.get_total_frames(self.video_file)
        self.source_framerate = self.get_fps(self.video_file)
        self.frame_capture = os.path.join(working_dir, f"{prefix}.png")

    def get_total_frames(self, video_file: str) -> int:
        cmd = f"ffmpeg -i {video_file} -vcodec copy -f rawvideo -y /dev/null 2>&1 | tr ^M '\n' | awk '/^frame=/ {{print $2}}'|tail -n 1"
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, check=True, shell=True)
        return int(result.stdout)

    def get_fps(self, video_file: str) -> int:
        cmd = f'ffprobe {video_file} 2>&1| grep ",* fps" | cut -d "," -f 5 | cut -d " " -f 2'
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, check=True, shell=True)
        return int(result.stdout)

    def harvest_frame(self, frame_count: int, video_in: str | None = None, frame_out: str | None = None, framerate: int | None = None) -> bool:
        v_in = video_in or self.video_file
        f_out = frame_out or self.frame_capture
        v_frameRate = framerate or self.source_framerate

        cadence = 1000/v_frameRate
        frame_millis = frame_count * cadence
        millis=int(frame_millis%1000)
        seconds=int((frame_millis/1000)%60)
        minutes=int((frame_millis/(1000*60))%60)
        hours=int((frame_millis/(1000*60*60))%24)
        timestamp = str(hours) + ":" + str(minutes) + ":" + str(seconds) + "." + str(millis)

        cmd = f'/usr/bin/ffmpeg -y -ss "{timestamp}" -i {v_in} -frames:v 1 {f_out}'
        print(cmd)
        try:
            subprocess.run(cmd, shell=True)
            return True
        except:
            print("FFMPEG failed to grab a frame")
            return False


class SlowMovie:
    def __init__(self, source_yaml: str | None = None, hardware_yaml: str | None = None, working_dir: str | None = None):
        '''
        Get framecount (minus one for zero index:
        ffmpeg -i input.mp4 -map 0:v:0 -c copy -f null -
        '''
        self.working_dir = working_dir or os.path.dirname(os.path.realpath(__file__))

        with open(source_yaml or os.path.join(self.working_dir, 'slowmovie-source.yml'), 'r') as file:
            source_config = yaml.safe_load(file)
        with open(hardware_yaml or os.path.join(self.working_dir, 'slowmovie-hardware.yml'), 'r') as file:
            hardware_config = yaml.safe_load(file)

        self.prefix = source_config['movie'].get('prefix', 'frame') # Optional YAML value for naming files
        self.frame_divisor = source_config['movie'].get('frame_divisor', 5) # Optional YAML value for number of frames to skip each run
        self.screens = hardware_config['screen_sizes']

        self.video = SourceVideo(source_config['movie']['video_file'], self.prefix, self.working_dir)


        '''
        Everything will happen in the working directory (remember trailing slash!).
        Make a symlink to the video in this directory
        '''
        self.mqtt_broker_addr = "192.168.1.135"
        self.mqtt_topic = "slowmovie/frame"

        #Don't edit these:
        self.framecount_json = os.path.join(self.working_dir, f"{self.prefix}_count.json")

    def process_next_frame(self):
        '''
        Workflow:
        * lookup next frame number
        * grab frame
        * convert to XBM
        * flip endianness and invert
        * publish to MQTT
        * increment framecount and save back to json
        '''

        #Import JSON
        framecount = self.get_saved_frame_count(self.framecount_json)
        if framecount == None:
            #Error getting JSON, try to generate a new one
            print("Trying to generate new JSON file")
            framecount = {'totalframes': self.video.total_frames, 'nextframe': 0}
            if (self.save_frame_count(self.framecount_json, framecount) == False):
                print("Abort: JSON file cannot be saved")
                return

        #Grab next frame
        if self.video.harvest_frame(framecount['nextframe']) == False:
            print("Abort: Unable to grab next frame from video")
            return

        #Convert to PBM
        conversion_count = 0
        for screen in self.screens:
            outfile = os.path.join(self.working_dir, f"{self.prefix}-{screen['name']}.pbm")
            if self.convert_to_pbm(self.video.frame_capture, outfile, screen['x'], screen['y']) == False:
                print(f"Abort: Unable to convert captured frame to XBM for screen: {screen['name']}")
            else:
                conversion_count += 1
        if conversion_count == 0:
            print("Abort: Unable to convert captured frame to any supplied screen size")
            return


        #Publish message to MQTT
        self.publish_mqtt(self.mqtt_broker_addr, self.mqtt_topic, str(datetime.datetime.now()))

        #Increment framecount and save
        framecount['nextframe'] += self.frame_divisor
        if framecount['nextframe'] >= framecount['totalframes']:
            framecount['nextframe'] = 0
        if self.save_frame_count(self.framecount_json, framecount) == False:
            print("Abort: failed to save new framecount")
            return

    def get_saved_frame_count(self, jsonfile):
        #Import JSON to get next frame count
        try:
            with open(jsonfile) as f:
                framecount = json.load(f)
        except:
            print("Unable to open JSON file %s" % self.framecount_json)
            return None
        return framecount


    def save_frame_count(self, jsonfile, count_dict):
        #Export JSON for frame count
        try:
            with open(jsonfile, 'w') as f:
                json.dump(count_dict, f)
            print("Successfully generated JSON file")
        except:
            print("Unable to write JSON file %s" % self.framecount_json)
            return False
        return True

    def convert_to_pbm(self, input_image: str, output_image: str, x_size: int, y_size: int, rotate: int = 0) -> bool:
        cmd = (
                f'convert {input_image} -gravity center +repage -rotate {rotate} '
                f'-resize "{x_size}x{y_size}" -gravity center -crop {x_size}x{y_size}+0+0 '
                f'-background black -extent "{x_size}x{y_size}" -colorspace Gray -gamma 1.2 '
                f'-sharpen 0x2 -dither FloydSteinberg -negate {output_image}'
            )
        print(cmd)

        try:
            subprocess.run(cmd, shell=True)
            return True
        except:
            print("Failed to convert image to XBM")
            return False

    def publish_mqtt(self, broker,topic,message):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        client.connect(broker)
        client.publish(topic, message)
        client.disconnect()

if __name__ == "__main__":
    frame_getter = SlowMovie()
    frame_getter.process_next_frame()
