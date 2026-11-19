#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <opencv2/core/core.hpp>
#include <opencv2/core/cuda.hpp> // Required for Jetson CUDA memory maps
#include "System.h"
#include "MapPoint.h"

namespace py = pybind11;

class pyslam {
private:
  ORB_SLAM2::System* slam;
  py::module_ np;

public:
  // Static numpy module declaration accessible within pyslam objects
  
  pyslam(const std::string &vocFile, const std::string &settingsFile,
	 int sensorType, bool bUseViewer) : np(py::module_::import("numpy")){
    // Initialize ORB-SLAM2
    // Set the third argument (bUseViewer) to false to prevent
    // Pangolin from crashing Jetson headless environments
    slam = new ORB_SLAM2::System(vocFile, settingsFile,
				 (ORB_SLAM2::System::eSensor)sensorType,
				 bUseViewer);
  }
  
  ~pyslam() {
    if (slam) {
      slam->Shutdown();
      delete slam;
      slam = nullptr;
    }

    delete this;
  }
  
  // Accepting memory pointers directly avoids Python object copying overhead
  void trackMonocularZeroCopy(uintptr_t dataPtr, int rows, int cols,
			      int type, double timestamp) {
    // Release the GIL so Python event loop runs unimpeded while CUDA processes
    py::gil_scoped_release release;
	
    // Wrap raw Jetson memory pointer directly into a cv::Mat without copying
    cv::Mat img(rows, cols, type, reinterpret_cast<void*>(dataPtr));
    
    // Pass to the CUDA tracking pipeline
    slam->TrackMonocular(img, timestamp);
  }
  
  py::array_t<uint8_t> trackAndReturnFrame(uintptr_t dataPtr, int rows,
					   int cols, int type,
					   double timestamp) {
    py::gil_scoped_release release;
    
    // Wrap the incoming Python frame pointer 
    cv::Mat img(rows, cols, type, reinterpret_cast<void*>(dataPtr));

    int tracking_state = slam->GetTrackingState();
    if (tracking_state == 4) { 
      std::cout << "Tracking loss detected. Resetting..." << std::endl;
      slam->Reset();
    }
    else if (tracking_state == 3) {
      int num_matches = slam->GetTrackedMapPoints().size();
      if (num_matches < 10) {
	cout << "Ghost tracking detected (" << num_matches << " pts)" << endl;
	slam->Reset();
      }
    }
    
    // Process tracking via the core CUDA/C++ engine
    slam->TrackMonocular(img, timestamp);
    
    // Safely call the native ORB-SLAM2 internal frame drawer
    // This generates a cv::Mat containing green tracking squares on features
    if(slam->mpFrameDrawer) {
      cv::Mat trackedFrame = slam->mpFrameDrawer->DrawFrame();
      if(!trackedFrame.empty()) {
	
	// Re-acquire the GIL briefly to securely allocate a
	// native Python array object
	py::gil_scoped_acquire acquire;
        
	// This natively translates data, shape sizes, and stride paths
	// perfectly for Python
	return py::array_t<uint8_t>
	  (
	   // Shape	   
	   {trackedFrame.rows, trackedFrame.cols, trackedFrame.channels()},
	   // Strides
	   {trackedFrame.step[0], trackedFrame.step[1], sizeof(uint8_t)},
	   // Data Pointer
	   trackedFrame.data
	   );
      }
    }

    py::gil_scoped_acquire acquire;
    return py::array_t<uint8_t>(); // Return empty NumPy array if tracking fails
  }

  // Wrapper function to extract map points as a Nx3 2D-array of floats
  // [[x1, y1, z1], [x2, y2, z2], ...]
  py::array_t<float> GetMapPoints() {

    if (!slam) {
      std::cout << "[DEBUG] C++ Object Pointer Address: " << this << std::endl;
      return py::array_t<float>();
    }

    std::vector<ORB_SLAM2::MapPoint*> vpMPs;
    size_t valid_count = 0;
    
    // Fetch all map points using the C++ pointer method
    {
      py::gil_scoped_release release;
      
      vpMPs = slam->GetmpMapAllMapPoints();

      // First, count valid, non-bad map points to size our array precisely
      for (size_t i = 0; i < vpMPs.size(); i++) {
        if (vpMPs[i] && !vpMPs[i]->isBad()) {
	  valid_count++;
        }
      }
    } // GIL is automatically reaquired leaving this code block

    // Pre-allocate continuous memory for a 2D NumPy array
    // (Shape: Valid_Count x 3)
    // This maps directly to a NumPy array of shape (N, 3)
    py::array_t<float, py::array::c_style> points_array({valid_count, (size_t)3});

    py::buffer_info buf_info = points_array.request();
    float* ptr = static_cast<float*>(buf_info.ptr);    

    // Get a direct writeable pointer to the underlying raw data buffer
    //float* ptr = static_cast<float*>(points_array.request().ptr);

    // Next pass: Safely lock and copy coordinates into the buffer
    {
      py::gil_scoped_release release;
      
      size_t idx = 0;
      for (size_t i = 0; i < vpMPs.size(); i++) {
        ORB_SLAM2::MapPoint* pMP = vpMPs[i];
        if (pMP && !pMP->isBad()) {
	  cv::Mat pos = pMP->GetWorldPos();
          
	  // Read types safely according to OpenCV compilation precision
	  if (pos.type() == CV_32F) {
	    ptr[idx * 3 + 0] = pos.at<float>(0); // X
	    ptr[idx * 3 + 1] = pos.at<float>(1); // Y
	    ptr[idx * 3 + 2] = pos.at<float>(2); // Z
	  } else if (pos.type() == CV_64F) {
	    ptr[idx * 3 + 0] = static_cast<float>(pos.at<double>(0));
	    ptr[idx * 3 + 1] = static_cast<float>(pos.at<double>(1));
	    ptr[idx * 3 + 2] = static_cast<float>(pos.at<double>(2));
	  }
	  if (!(isfinite(ptr[idx * 3 + 0]) &&
	        isfinite(ptr[idx * 3 + 1]) &&
	        isfinite(ptr[idx * 3 + 2]))) {
	      cout << "Found bad value in map points" << endl;
	    }
	  idx++;
        }
      }
    }

    return points_array;
  }

  // Wrapper function to extract keyframe points as a Nx3 2D-array of floats
  // [[x1, y1, z1], [x2, y2, z2], ...]
  py::array_t<float> GetKeyFramePoints() {

    if (!slam) {
      std::cout << "[DEBUG] C++ Object Pointer Address: " << this << std::endl;
      return py::array_t<float>();
    }

    vector<ORB_SLAM2::KeyFrame*>kfs = slam->GetMap()->GetAllKeyFrames();
    size_t num_keyframes = kfs.size();
    
    if (num_keyframes == 0) {
      std::cout << "[DEBUG] No keyframes yet " << this << std::endl;
      return py::array_t<float>();
    }
    
    // Pre-allocate continuous memory for a 2D NumPy array
    // (Shape: number_of_keyframes x 3)
    // This maps directly to a NumPy array of shape (N, 3)
    py::array_t<float> points_array({(size_t)num_keyframes, (size_t)3});    

    // Get a direct writeable pointer to the underlying raw data buffer
    float* ptr = static_cast<float*>(points_array.request().ptr);

    // Copy coordinates into the buffer
    {
      py::gil_scoped_release release;

      size_t idx = 0;
      for (size_t i = 0; i < num_keyframes; i++) {
        ORB_SLAM2::KeyFrame* pKF = kfs[i];
        if (pKF) {
	  cv::Mat pose = pKF->GetPoseInverse();
          
	  // Read types safely according to OpenCV compilation precision
	  if (pose.type() == CV_32F) {
	    ptr[idx * 3 + 0] = pose.at<float>(0,3); // X
	    ptr[idx * 3 + 1] = pose.at<float>(1,3); // Y
	    ptr[idx * 3 + 2] = pose.at<float>(2,3); // Z
	  } else if (pose.type() == CV_64F) {
	    ptr[idx * 3 + 0] = static_cast<float>(pose.at<double>(0,3));
	    ptr[idx * 3 + 1] = static_cast<float>(pose.at<double>(1,3));
	    ptr[idx * 3 + 2] = static_cast<float>(pose.at<double>(2,3));
	  }
	  if (!(isfinite(ptr[idx * 3 + 0]) &&
	        isfinite(ptr[idx * 3 + 1]) &&
	        isfinite(ptr[idx * 3 + 2]))) {
	      cout << "Found bad value in keyframe points" << endl;
	    }
	  idx++;
        }
      }
    }

    return points_array;
  }

  // Get the current camera pose matrix (4x4) 2D-array of floats
  py::array_t<float>  GetCurrentPose() {

    if (!slam) {
      std::cout << "[DEBUG] C++ Object Pointer Address: " << this << std::endl;
      return null_transform();
    }
    
    {
      py::gil_scoped_release release;

      if (slam->GetLocalMapper() == nullptr)
	return null_transform();

      if (slam->GetLocalMapper()->GetCurrentKeyFrame() == nullptr)
	return null_transform();
    
      cv::Mat currPose =
	slam->GetLocalMapper()->GetCurrentKeyFrame()->GetPose();

      if (currPose.empty())
	return null_transform();
    
      // Pre-allocate continuous memory for a 2D NumPy array
      // This maps directly to a NumPy array of shape (4, 4)
      py::array_t<float> pose({(size_t)4,(size_t)4});

      // Get a direct writeable pointer to the underlying raw data buffer
      // float* ptr = static_cast<float*>(pose.request().ptr);
      auto r = pose.mutable_unchecked<2>();    
      
      // Populate Rotation (top-left 3x3) and Translation (top-right 3x1)
      for (int i = 0; i < 4; ++i) {
	for (int j = 0; j < 4; ++j) {
	  r(i, j) = currPose.at<float>(i, j);
	}
      }

      return pose;
    }
  }

  py::array_t<float> null_transform() {
    py::array_t<float> arr({4, 4});
    auto r = arr.mutable_unchecked<2>();
    
    // Row 0
    r(0,0) = 1.0f; r(0,1) = 0.0f; r(0,2) = 0.0f; r(0,3) = 0.0f;
    // Row 1
    r(1,0) = 0.0f; r(1,1) = 1.0f; r(1,2) = 0.0f; r(1,3) = 0.0f;
    // Row 2
    r(2,0) = 0.0f; r(2,1) = 0.0f; r(2,2) = 1.0f; r(2,3) = 0.0f;
    // Row 3
    r(3,0) = 0.0f; r(3,1) = 0.0f; r(3,2) = 0.0f; r(3,3) = 0.0f;
    
    return arr;
  }  

  int GetTrackingState()
  {
    return slam->GetTrackingState();
  }

  // Expose explicit shutdown functionality
  void shutdown() {
    
    // Release the GIL so Python doesn't stall while C++ thread joins occur
    py::gil_scoped_release release;
        
    if (slam != nullptr) {
      // Native C++ call that stops all internal threads safely      
      slam->Shutdown(); 
      delete slam;
      slam = nullptr;
    }
  }

  py::array_t<double> multiply_matrices(py::array_t<double> a,
					py::array_t<double> b) {
    py::object result = np.attr("matmul")(a, b);
    return result.cast<py::array_t<double>>();
  }

  int GetNumKeyFrames()
  {
    return slam->GetMap()->KeyFramesInMap();
  }

  void SetHidePangolinDisplay(bool value)
  {
    if (slam->GetViewer() != nullptr)
      slam->GetViewer()->SetHidePangolinDisplay(value);
    else
      cout << "GetViewer returned null" << endl;
  }

  void SetMapSize(int width, int height)
  {
    if (slam->GetViewer() != nullptr)
      slam->GetViewer()->SetRenderSize(width, height);
    else
      cout << "GetViewer returned null" << endl;
  }

  py::array_t<uint8_t> GetRenderedMap()
  {
    cv::Mat rmap = slam->GetViewer()->GetRenderedFrame();

   // Ensure the matrix is continuous and 8-bit unsigned (e.g., CV_8UC3 or CV_8UC1)
    cv::Mat continuous_mat = rmap.isContinuous() ? rmap : rmap.clone();

    std::vector<ssize_t> shape;
    std::vector<ssize_t> strides;

    if (continuous_mat.channels() == 1) {
        shape = { continuous_mat.rows, continuous_mat.cols };
        strides = { static_cast<ssize_t>(continuous_mat.step[0]), sizeof(uint8_t) };
    } else {
        shape = { continuous_mat.rows, continuous_mat.cols, continuous_mat.channels() };
        strides = { 
            static_cast<ssize_t>(continuous_mat.step[0]), 
            static_cast<ssize_t>(continuous_mat.step[1]), 
            sizeof(uint8_t) 
        };
    }

    // Returning without a base object copies the data into a new NumPy array safely
    return py::array_t<uint8_t>(shape, strides, continuous_mat.data);    
  }
};

PYBIND11_MODULE(pyslam, m) {

  // Bind the pyslam class and its member methods  
  py::class_<pyslam, std::unique_ptr<pyslam, py::nodelete>>(m, "pyslam")      
    .def(py::init<const std::string&, const std::string&, int, bool>())
    .def("track_monocular", &pyslam::trackMonocularZeroCopy)
    .def("track_frame", &pyslam::trackAndReturnFrame)
    .def("get_map_points", &pyslam::GetMapPoints)
    .def("get_keyframe_points", &pyslam::GetKeyFramePoints)
    .def("get_current_pose", &pyslam::GetCurrentPose)
    .def("get_tracking_state",&pyslam::GetTrackingState)
    .def("get_num_keyframes",&pyslam::GetNumKeyFrames)
    .def("set_hide_pangolin_display",&pyslam::SetHidePangolinDisplay)
    .def("set_map_size",&pyslam::SetMapSize)
    .def("get_rendered_map",&pyslam::GetRenderedMap)
    .def("shutdown", &pyslam::shutdown); // Bind the shutdown method

  // Also bind the Sensor Enum so Python understands the constructor argument
  py::enum_<ORB_SLAM2::System::eSensor>(m, "Sensor")
    .value("MONOCULAR", ORB_SLAM2::System::MONOCULAR)
    .value("STEREO", ORB_SLAM2::System::STEREO)
    .value("RGBD", ORB_SLAM2::System::RGBD)
    .export_values();
}    

