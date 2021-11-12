//This module provides the main interface for the shadow propagation system
//Authors: Bryan Poling
//Copyright (c) 2021 Sentek Systems, LLC. All rights reserved. 
#pragma once

//System Includes
#include <vector>
#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <iostream>

//External Includes
#include <opencv2/opencv.hpp>

//Project Includes
#include "../../EigenAliases.h"
#include "../Shadow-Detection/ShadowDetection.hpp"
#include <torch/script.h>
#include <torch/torch.h>

namespace ShadowPropagation {
	//Class for a time-stamped, geo-registered time available function - each pixel corresponds to a patch of ground. We use type uint16_t and
	//interperet the value for a pixel as the number of seconds that patch of ground is expected to be free of shadows (measured from the timestamp).
	//If we don't see anything in our forcast that is expected to hit a given pixel then the pixel is predicted to be clear for the full prediction
	//horizon. That horizon is different for different pixels though (e.g. a pixel on our periphery may have almost no prediction horizon), and it may not
	//be easy to estimate the horizon for a given pixel (it depends on cloud speed and direction). It is likily also not very actionable information so
	//to avoid the added complexity of trying to estimate this we will just use a sentinal value to indicate this condition (that nothing currently visible
	//is expected to hit a given pixel). We will use std::numeric_limits<uint16_t>::max() to indicate this.
	class TimeAvailableFunction {
		public:
			EIGEN_MAKE_ALIGNED_OPERATOR_NEW
			using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
			
			cv::Mat TimeAvailable; //Raster data. Type: uint16_t, and values represents time available (in seconds).
			Eigen::Vector2d UL_LL; //(Latitude, Longitude) of center of upper-left pixel, in radians
			Eigen::Vector2d UR_LL; //(Latitude, Longitude) of center of upper-right pixel, in radians
			Eigen::Vector2d LL_LL; //(Latitude, Longitude) of center of lower-left pixel, in radians
			Eigen::Vector2d LR_LL; //(Latitude, Longitude) of center of lower-right pixel, in radians
			TimePoint Timestamp;
	};
	
	//Singleton class for the shadow propagation system - We use a callback system to ensure that every new shadow map that is computed is received by
	//this function (even if we are falling behind real-time in processing). We use a similar mechanism in the shadow detection module to ensure that
	//it does not miss frames from the drone feed. This is all because the shadow propagation module may use models that don't handle missing data well,
	//such as LSTM networks. Note, however, that the shadow propagation module does not offer callbacks of its own because downstream users (i.e. the
	//Guidance module) do not have the same limitation and can just poll for the latest TA function whenever they need it.
	class ShadowPropagationEngine {
		private:
			std::thread       m_engineThread;
			bool              m_running;
			std::atomic<bool> m_abort;
			std::mutex        m_mutex;
			static const int  TARGET_INPUT_LENGTH = 15;
			static const int  TIME_HORIZON = 15;
			static constexpr double OUTPUT_THRESHOLD = 0.4;

			int               m_callbackHandle; //Handle for this objects shadow detection engine callback
			std::unordered_map<int, std::function<void(TimeAvailableFunction const & TA)>> m_callbacks;
			
			std::vector<ShadowDetection::InstantaneousShadowMap> m_unprocessedShadowMaps;

			TimeAvailableFunction m_TimeAvail; //Most recent time available function
            torch::jit::script::Module m_module; // TorchScript Model
            torch::Device m_device;
            //std::deque<torch::Tensor> m_prevInputs;
            std::Edeque<Eigen::MatrixXf> m_inputHist;
            int counter;

            bool isSaved;
			
			void ModuleMain(void);
			
		public:
			EIGEN_MAKE_ALIGNED_OPERATOR_NEW
			using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
            int numImagesProcessed;
            int numMicroseconds;

        static ShadowPropagationEngine & Instance() { static ShadowPropagationEngine Obj; return Obj; }
			
			//Constructors and Destructors
			ShadowPropagationEngine() : m_running(false), m_abort(false), 
										m_device(torch::cuda::is_available() ? torch::kCUDA : torch::kCPU) {
				m_engineThread = std::thread(&ShadowPropagationEngine::ModuleMain, this);
				if (m_device.is_cuda()) {
                	m_module = torch::jit
                        ::load(Handy::Paths::ThisExecutableDirectory().parent_path()
                        .string().append("/SRC/Modules/Shadow-Propagation/model_cuda.pt"), m_device);
				} else {
                	m_module = torch::jit
                        ::load(Handy::Paths::ThisExecutableDirectory().parent_path()
                        .string().append("/SRC/Modules/Shadow-Propagation/model.pt"), m_device);

				}
                m_module.eval();
            }
			~ShadowPropagationEngine() {
				m_abort = true;
				if (m_engineThread.joinable())
					m_engineThread.join();
			}
			
			inline void Start(void);     //Start or restart continuous processing of new shadow maps
			inline void Stop(void);      //Stop processing
			inline bool IsRunning(void); //Returns true if running, false if stopped
			
			//Callback access
			inline int  RegisterCallback(std::function<void(TimeAvailableFunction const & TA)> Callback); //Regester callback for new TA functions
			inline void UnRegisterCallback(int Handle); //Unregister callback for new TA functions (input is token returned by RegisterCallback()
			
			//Accessors - Each returns false if no Time Available functions have been computed yet
			inline bool GetTimestampOfMostRecentTimeAvailFun(TimePoint & Timestamp);
			inline bool GetMostRecentTimeAvailFun(TimeAvailableFunction & TimeAvailFun);
	};

	inline void ShadowPropagationEngine::Start() {
		std::scoped_lock lock(m_mutex);
		m_unprocessedShadowMaps.clear(); //Ditch any old unprocessed data in the buffer
		//Note: Also reset any state data relating to internal models
		if (m_running)
			return;
		//Register a callback for handling new shadow maps. Note that our callback just copies the data to a buffer - we don't do any actual
		//processing here or it would hold up the shadow detection module. The heavy lifting is done in ModuleMain()
		m_callbackHandle = ShadowDetection::ShadowDetectionEngine::Instance().RegisterCallback([this](ShadowDetection::InstantaneousShadowMap const & NewMap) {
			std::scoped_lock lock(m_mutex);
			m_unprocessedShadowMaps.push_back(NewMap);
		});
		m_running = true;
	}

	inline void ShadowPropagationEngine::Stop(void) {
		std::scoped_lock lock(m_mutex);
		if (! m_running)
			return;
		ShadowDetection::ShadowDetectionEngine::Instance().UnRegisterCallback(m_callbackHandle);
		m_running = false;
	}

	inline bool ShadowPropagationEngine::IsRunning(void) {
		std::scoped_lock lock(m_mutex);
		return m_running;
	}

	//Regester callback for new TA functions (returns handle)
	inline int ShadowPropagationEngine::RegisterCallback(std::function<void(TimeAvailableFunction const & TA)> Callback) {
		std::scoped_lock lock(m_mutex);
		int token = 0;
		while (m_callbacks.count(token) > 0U)
			token++;
		m_callbacks[token] = Callback;
		return token;
	}
	
	//Unregister callback for new TA functions (input is token returned by RegisterCallback()
	inline void ShadowPropagationEngine::UnRegisterCallback(int Handle) {
		std::scoped_lock lock(m_mutex);
		m_callbacks.erase(Handle);
	}

	inline bool ShadowPropagationEngine::GetTimestampOfMostRecentTimeAvailFun(TimePoint & Timestamp) {
		std::scoped_lock lock(m_mutex);
		if ((m_TimeAvail.TimeAvailable.rows == 0) || (m_TimeAvail.TimeAvailable.cols == 0))
			return false;
		Timestamp = m_TimeAvail.Timestamp;
		return true;
	}

	inline bool ShadowPropagationEngine::GetMostRecentTimeAvailFun(TimeAvailableFunction & TimeAvailFun) {
		std::scoped_lock lock(m_mutex);
		if ((m_TimeAvail.TimeAvailable.rows == 0) || (m_TimeAvail.TimeAvailable.cols == 0))
			return false;
		TimeAvailFun = m_TimeAvail;
		return true;
	}
}




