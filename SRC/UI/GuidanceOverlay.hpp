//The guidance overlay is used by the Guidance module to draw guidance-related data on the map widget
//Author: Bryan Poling
//Copyright (c) 2021 Sentek Systems, LLC. All rights reserved. 
#pragma once

//External Includes
#include "../HandyImGuiInclude.hpp"

//Project Includes
#include "../EigenAliases.h"
#include "../Polygon.hpp"

class GuidanceOverlay {
	private:
		std::mutex m_mutex;
		std::Evector<PolygonCollection> m_SurveyRegionPartition;
		std::Evector<std::Evector<Triangle>> m_SurveyRegionPartitionTriangulation;
		std::string m_GuidanceMessage_1;
		std::string m_GuidanceMessage_2;
		std::string m_GuidanceMessage_3;
		
	public:
		 GuidanceOverlay() = default;
		~GuidanceOverlay() = default;
		
		//Called in the draw loop for the map widget
		void Draw_Overlay(Eigen::Vector2d const & CursorPos_NM, ImDrawList * DrawList, bool CursorInBounds);
		void Draw_MessageBox(Eigen::Vector2d const & CursorPos_NM, ImDrawList * DrawList, bool CursorInBounds);
		
		//Data Setter Methods - you can provide a partition of a survey region and/or messages to display in a box. Messages persist until changed or cleared,
		//so this can be useful for state info (instead of just printing to a terminal and having it lost).
		//A partition of a survey region is provided as a vector of polygon collection. Each element in the vector represents a component of the partition.
		//All points in the provided partition objects should be in Normalized Mercator.
		void Reset();
		void SetSurveyRegionPartition(std::Evector<PolygonCollection> const & Partition); //Set the partition of the survey region to draw
		void ClearSurveyRegionPartition();                                                //Clear/delete the partition of the survey region
		void SetGuidanceMessage1(std::string const & Message); //Display optional message in box on map (give empty string to disable)
		void SetGuidanceMessage2(std::string const & Message); //Display optional message in box on map (give empty string to disable)
		void SetGuidanceMessage3(std::string const & Message); //Display optional message in box on map (give empty string to disable)
};




