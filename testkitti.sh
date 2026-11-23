#!/bin/bash

cd build

SEQDIR="../../Odometry/dataset/sequences/05/"

if [ ! -d "$SEQDIR" ]; then
    echo "KITTI sequence directory $SEQDIR does not exist."
    echo "Download the KITTI grayscale odometry evaluation set from:"
    echo "https://www.cvlibs.net/datasets/kitti/eval_odometry.php"
    exit 1
fi

./mono_kitti ../Vocabulary/ORBvoc.bin ../Examples/Monocular/KITTI04-12.yaml $SEQDIR true
