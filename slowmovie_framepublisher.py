#!/usr/bin/env python

"""
slowmovie


MIT License

Copyright (c) 2020-2024 Mike Szczys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""

import re
import subprocess
import json
import paho.mqtt.client as mqtt
import datetime
import yaml

class SlowMovie:
    def __init__(self, source_yaml: str | None = None, hardware_yaml: str | None = None):
        '''
        Get framecount (minus one for zero index:
        ffmpeg -i input.mp4 -map 0:v:0 -c copy -f null -
        '''
        self.workingDir = "/home/mike/compile/slowmovie/"

        with open(source_yaml or f'{self.workingDir}slowmovie-source.yml', 'r') as file:
            source_config = yaml.safe_load(file)
        with open(hardware_yaml or f'{self.workingDir}slowmovie-hardware.yml', 'r') as file:
            hardware_config = yaml.safe_load(file)


        self.totalFrames = source_config['movie']['totalFrames']
        self.sourceFrameate = source_config['movie']['sourceFrameRate']
        self.frame_divisor = source_config['movie']['frame_divisor'] #How many frames to wait before pushing new image to display
        self.screensize_x = hardware_config['screen_sizes'][0]['x']
        self.screensize_y = hardware_config['screen_sizes'][0]['y']

        '''
        Everything will happen in the working directory (remember trailing slash!).
        Make a symlink to the video in this directory
        '''
        self.videoFile = source_config['movie']['videoFile']
        self.mqttBrokerAddr = "192.168.1.135"
        self.mqttTopic = "slowmovie/frame"

        #Don't edit these:
        self.framecountJSON = self.workingDir + "framecount.json"
        self.videoFile = self.workingDir + self.videoFile
        self.frameCapture = self.workingDir + "frame.png"
        self.inputXBMfile = self.workingDir + "frame.xbm"
        self.inputPBMfile = self.workingDir + "frame.pbm"

    def processNextFrame(self):
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
        framecount = self.getSavedFramecount(self.framecountJSON)
        if framecount == None:
            #Error getting JSON, try to generate a new one
            print("Trying to generate new JSON file")
            framecount = {'totalframes': self.totalFrames, 'nextframe': 0}
            if (self.saveFramecount(self.framecountJSON, framecount) == False):
                print("Abort: JSON file cannot be saved")
                return

        #Grab next frame
        if self.harvestFrame(self.videoFile, self.sourceFrameate, framecount['nextframe']) == None:
            print("Abort: Unable to grab next frame from video")
            return

        #Convert to PBM
        if self.convertToPBM(self.frameCapture, self.screensize_x, self.screensize_y) == None:
            print("Abort: Unable to convert captured frame to XBM")
            return

        #Publish message to MQTT
        self.publishMQTT(self.mqttBrokerAddr, self.mqttTopic, str(datetime.datetime.now()))

        #Increment framecount and save
        framecount['nextframe'] += self.frame_divisor
        if framecount['nextframe'] >= framecount['totalframes']:
            framecount['nextframe'] = 0
        if self.saveFramecount(self.framecountJSON, framecount) == False:
            print("Abort: failed to save new framecount")
            return

    def getSavedFramecount(self, jsonfile):
        #Import JSON to get next frame count
        try:
            with open(jsonfile) as f:
                framecount = json.load(f)
        except:
            print("Unable to open JSON file %s" % self.framecountJSON)
            return None
        return framecount


    def saveFramecount(self, jsonfile, countDict):
        #Export JSON for frame count
        try:
            with open(jsonfile, 'w') as f:
                json.dump(countDict, f)
            print("Successfully generated JSON file")
        except:
            print("Unable to write JSON file %s" % self.framecountJSON)
            return False
        return True

    def harvestFrame(self, video, frameRate, frameCount):
        cadence = 1000/frameRate
        frameMilliseconds = frameCount * cadence
        millis=int(frameMilliseconds%1000)
        seconds=int((frameMilliseconds/1000)%60)
        minutes=int((frameMilliseconds/(1000*60))%60)
        hours=int((frameMilliseconds/(1000*60*60))%24)
        timestamp = str(hours) + ":" + str(minutes) + ":" + str(seconds) + "." + str(millis)

        cmd = '/usr/bin/ffmpeg -y -ss "' + timestamp + '" -i ' + self.videoFile + ' -frames:v 1 ' + self.frameCapture
        print(cmd)
        try:
            subprocess.run(cmd, shell=True)
            return True
        except:
            print("FFMEG failed to grab a frame")
            return None

    def convertToPBM(self, image, x_size, y_size, rotate=0):
        cmd = (
                f'convert {self.frameCapture} -gravity center +repage -rotate {rotate} '
                f'-resize "{x_size}x{y_size}" -gravity center -crop {x_size}x{y_size}+0+0 '
                f'-background black -extent "{x_size}x{y_size}" -colorspace Gray -gamma 1.2 '
                f'-sharpen 0x2 -dither FloydSteinberg -negate {self.inputPBMfile}'
            )
        print(cmd)

        try:
            subprocess.run(cmd, shell=True)
            return True
        except:
            print("Failed to convert image to XBM")
            return None

    def publishMQTT(self, broker,topic,message):
        mqttBroker = broker
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        client.connect(mqttBroker)
        client.publish(topic, message)
        client.disconnect()

if __name__ == "__main__":
    frame_getter = SlowMovie()
    frame_getter.processNextFrame()
