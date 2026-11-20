import cv2
import numpy as np

def drawmap(height, width, pose, mappts_3d, kfpts_3d):

    # Create the base image
    img = np.zeros((height, width, 3), dtype=np.uint8)

    # Fill the image with white
    img[:] = [255, 255, 255]

    # Guard against empty or invalid input structures
    if mappts_3d is None or len(mappts_3d) == 0:
        return img

    # Extract Rotation (3x3) and Translation (3, 1) from the 4x4 pose
    R = pose[:3, :3]
    t = pose[:3,  3]

    # Trim out any unusable values from the map points
    valid_mask = np.all(np.isfinite(mappts_3d),  axis=1) & \
                 np.all(np.abs(mappts_3d) < 100, axis=1)
    map_points = mappts_3d[valid_mask]

    # Rotate then add the translation 
    proj_points = (map_points @ R.T) + t
    
    # Place the current bot position at the center of the image
    scale    = float(150.0)
    offset_x = float((width  / 2.0));
    offset_z = float((height / 2.0));

    x_coords = proj_points[:, 0]
    z_coords = proj_points[:, 2]

    xx_coords = scale * x_coords + offset_x
    zz_coords = height - (scale * z_coords + offset_z)
    
    coords = np.column_stack((xx_coords.ravel(), zz_coords.ravel()))
    
    for x, z in coords:
        # Let opencv handle any drawing out-of-bounds
        if (z > offset_z):
            cv2.circle(img, (int(x), int(z)), 1, (192, 0, 0), -1)
        else:
            cv2.circle(img, (int(x), int(z)), 1, (255, 0, 0), -1)

    # Add in the keyframe points
    
    # Rotate then add the translation 
    proj_points = (kfpts_3d @ R.T) + t

    x_coords = proj_points[:, 0]
    z_coords = proj_points[:, 2]

    xx_coords = scale * x_coords + offset_x
    zz_coords = height - (scale * z_coords + offset_z)
    
    coords = np.column_stack((xx_coords.ravel(), zz_coords.ravel()))
    
    for x, z in coords:
        # Let opencv handle any drawing out-of-bounds
        cv2.circle(img, (int(x), int(z)), 4, (0, 0, 255), 1)

    return img
