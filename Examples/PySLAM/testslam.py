import os
import pygame
import sys
import time
import cv2
import ctypes
import numpy as np
import pyslam
import sys
import simpleMap

# Start of main app

# Initialize pygame
pygame.init()

height = 480
width  = 640

screen = pygame.display.set_mode((width*2, height))
canvas = pygame.Surface((width*2, height))

# Define rectangles for the subareas of the display window
rect_left  = pygame.Rect(0,     0, width,   height)
rect_right = pygame.Rect(width, 0, width*2, height)

pygame.display.set_caption("Zynet Bot SLAM Tracking")

# Initialze orb-slam2
vocab_path    = "../../Vocabulary/ORBvoc.bin"
settings_path = "ZynetBot.yaml"

# The OrbSlam2 Viewer generates the map image
# Set this to false if you want to use simpleMap to generate the map image
enableOrbSlam2Viewer = True

# Enabling the OrbSlam2Viewer also displays both the overlayed input image
# and the generated map image in separate pop-up windows. Disabling the
# OrbSlam2 displays allows using the viewer to generate the map image,
# but prevents having the OrbSlam2 Viewer from displaying the images since
# we handle the displays using PyGame

disableOrbSlam2Displays = True

slam = pyslam.pyslam(vocab_path, settings_path, 0, enableOrbSlam2Viewer,
                     disableOrbSlam2Displays, width, height)

clock   = pygame.time.Clock()
running = True

laststamp = 0
count     = 0
accum     = 0.0

start = time.time()

framedir = "slam_testseq/image_0"

frame_count = 0
fps         = 5.0
time_step   = 1.0 / fps  # 0.2 seconds

try:
    
    for image_name in sorted(os.listdir(framedir)):
        image_path = os.path.join(framedir, image_name)
        frame      = cv2.imread(image_path)

        rows, cols, channels = frame.shape
        cv_type = cv2.CV_8UC3 if channels == 3 else cv2.CV_8UC1

        # C++ directly hands over a valid NumPy matrix object
        now          = time.time()
        #timestamp   = now - start
        timestamp    = frame_count * time_step
        frame_count += 1
        
        tracked_img = slam.track_frame(frame.ctypes.data, rows, cols,
                                       cv_type, timestamp)

        # Keep track of the frame rate
        now = time.time()
        if laststamp > 0:
            delta = now - laststamp
            accum = accum + delta
            count = count + 1

        laststamp = now
            
        if tracked_img is not None and tracked_img.size > 0:
            surface = pygame.surfarray.make_surface(tracked_img.swapaxes(0, 1))
        else:
            # Fallback to standard frame if SLAM tracking fails or drops out
            surface = pygame.surfarray.make_surface(frame.swapaxes(0, 1))

        screen.blit(surface, rect_left)

        # Create the projected scatter plot of the points
        if enableOrbSlam2Viewer:
            map_img = slam.get_rendered_map()
        else:
            # Get the current pose from the most recent frame
            pose = slam.get_current_pose()

            # Get the current set of mapping points
            map_points = slam.get_map_points()

            # Get the set of current and past keyframes
            kf_points = slam.get_keyframe_points()
            
            # Draw the map
            map_img = simpleMap.drawmap(height,width,pose,map_points,kf_points)

        # Display the map image on the right side of the split screen

        # Need to: flip about X, rotate 90 CCW, then flip about X again.
        # Instead:
        # map.img[::-1] flips the vertical axis first;
        # .swapaxes(0, 1) then transposes (H, W) -> (W, H)
        
        surface = pygame.surfarray.make_surface(map_img[::-1].swapaxes(0, 1))
        screen.blit(surface, rect_right)
        
        pygame.display.flip()

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:  # Press Esc to quit
                    running = False

        if running is not True:
            break
        
        # Limit the loop rate (approx. 20 updates per second)
        # time.sleep(incr_duration)
        # clock.tick(int(1000.0*incr_duration))
                
except KeyboardInterrupt:
    print("\nExiting...")
except OSError as e:
    print(f"OS error occurred while reading '{framedir}': {e}")    
finally:
    if (accum > 0):
        print("Average frame rate:",float(count) / accum);
    print("\nExiting...")
#   save_trajectory(slam.get_trajectory_points(), 'trajectory.txt')
    time.sleep(2)
    slam.shutdown()
    pygame.quit()
    sys.exit()
