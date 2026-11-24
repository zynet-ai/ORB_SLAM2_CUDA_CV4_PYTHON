# ORB_SLAM2_CUDA_CV4_PYTHON
The code in this repository builds a shared object library (libORB_SLAM2.so) along with python interfaces to that library (pyslam.cpython-36m-aarch64-linux-gnu.so) for use on a Jetson Nano.

It is derived from Thien Nguyen's ORB_SLAM2_CUDA (https://github.com/thien94/ORB_SLAM2_CUDA) which is a minimally modified
version of Yunchih Chen's ORB-SLAM2 with GPU enhancements (https://github.com/yunchih/ORB-SLAM2-GPU2016-final).
Chen's version is ultimately derived from Raul Mur-Artal's ORB-SLAM2 (https://github.com/raulmur/ORB_SLAM2).

The primary modifications here are:
- Updating the code base to use opencv 4.6 with both CUDA and gstreamer support
- Adding Python interfaces to the C++ ORB SLAM2 library (via pybind11)
- Fixups to the CMakeLists.txt to work with virtual environments (archiconda3)
- Adding hooks to access Pangolin rendered frames without using Pangolin displays. This allows, for example, displaying the two separately rendered frames using a single pygame surface
- Getting everything compiled and running on a Jetson Nano
- Adding Vocabulary format conversion apps to convert from text to binary format and from binary to text format. Using a binary format loads *much, much* faster
- Fixing the Thirdparty DBoW2 code to read the correct number of records from the vocabulary file in both binary and text formats
- Providing the Vocabulary file in binary format rather than text format
- Adding an example Python script along with a short test image sequence to exercise the Python interfaces. A simple map rendering Python module is also included as an alternative to using Pangolin. Choice of rendering methods is controlled within the example code. An image of running the Python example code with the test image sequence is shown below.

While the Nguyen/Chen codebase supports ROS (with ROS components carried along in this repository), we have not (yet) attempted to build or install ROS support. Our primary focus has been running this code under python/pygame on our 'bot, which is remotely accessed using NoMachine. We use pygame to provide joystick inputs as a means for training our 'bot using Learning By Example techniques.

<img width="1286" height="512" alt="botslam" src="https://github.com/user-attachments/assets/88e2a71b-7b2a-4dd3-9340-16f2d3a49ffe" />

# Installation on Jetson Nano

Check to get the CUDA compiler version:
```
nvcc --version 
```
If you get error ```nvcc: command not found ```, check [this page](https://devtalk.nvidia.com/default/topic/995277/cuda-8-0-toolkit-install-nvcc-not-found-ubuntu-16-04/) to solve it first before moving on.

### Build OpenCV 4.6 with CUDA and gstreamer support for Tegra

Use the provided shell script
```
nanoBuildInstallOpenCV.sh
```
This is derived from the shell script from Qengineering which can be found here

https://github.com/Qengineering/Install-OpenCV-Jetson-Nano/blob/main/OpenCV-4-6-0.sh

with several additional components (e.g., gstreamer support). It determines which Jetson platform you have, pulls in the needed dependencies, builds, and finally installs opencv 4.6.0.

### Install dependancies for ORB-SLAM2

#### Pangolin
Download and install instructions for Pangolin can be found at: https://github.com/stevenlovegrove/Pangolin. Basically

```
git clone --depth 1 --branch v0.8 https://github.com/stevenlovegrove/Pangolin
```
to grab v0.8.

In order to get the ORB SLAM2 running on our Jetson Nano where we use a headless configuration with remote access via noMachine, the Pangolin source file *display_headless.cpp* needs to be tweaked slightly to use EGL. The diff file is provided in this repo

#### BLAS and LAPACK
```
sudo apt-get install libblas-dev  
sudo apt-get install liblapack-dev  
```

#### Eigen3
Download and install instructions can be found at: http://eigen.tuxfamily.org. This ORB SLAM2 build requires at least 3.1.0.
We use 3.4.1

# Building ORB_SLAM2_CUDA_CV4_PYTHON
Clone this repo and execute the build script:
```
git clone https://github.com/zynet-ai/ORB_SLAM2_CUDA_CV4_PYTHON
cd ORB_SLAM2_CUDA_CV4_PYTHON
chmod +x build.sh
./build.sh
```
This builds the third party components DBoW2 and g2o, uncompresses the vocabulary file, then builds the ORB SLAM2 library, utilities, and components.

If you have problem with the build, sometimes it helps to remove previous build folders:
```
# in ORB_SLAM2_CUDA_CV4_PYTHON folder
sudo rm -rf build  
sudo rm -rf Thirdparty/DBoW2/build  
sudo rm -rf Thirdparty/g2o/build
./build.sh
```
When the build is completed, you can try the examples as in the ORB-SLAM2 repo's instructions.

Despite the lengthy name of this git repository, the resulting shared object library is named libORB_SLAM2.so and is installed in /usr/local/lib.

The python interface library is named pyslam.cpython-36m-aarch64-linux-gnu.so (since we're using Python 3.6) and is installed in /usr/lib/python3/dist-packages.

We then manually added a symbolic link to the python interface library in the lib/python3.6/site-packages directory in our virtual environment (again since we're using python 3.6)
