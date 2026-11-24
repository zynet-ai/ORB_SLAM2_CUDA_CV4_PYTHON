set -e

echo "Configuring and building Thirdparty/DBoW2 ..."

cd Thirdparty/DBoW2
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j1

cd ../../../

echo "Configuring and building Thirdparty/g2o ..."

cd Thirdparty/g2o

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j1

cd ../../../

echo "Uncompressing vocabulary ..."

cd Vocabulary
tar -xf ORBvoc.bin.tar.gz

cd ..

echo "Configuring and building ORB_SLAM2 ..."

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j1
sudo make install

cd ..

echo "Uncompressing test image sequence ..."

cd Examples/PySLAM
tar -xf slam_testseq.tgz

cd ../..
