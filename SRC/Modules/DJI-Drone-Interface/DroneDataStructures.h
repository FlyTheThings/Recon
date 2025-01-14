//This header provides basic data structures used for interacting with DJI drones (and some very small utilities)
//Author: Bryan Poling
//Copyright (c) 2022 Sentek Systems, LLC. All rights reserved.
#pragma once

//Project Includes
#include "../../Maps/MapUtils.hpp"

namespace DroneInterface {
	//VirtualStickMode has several different configuration settings that impact how each control is interpreted. We
	//only implement two combinations of settings, which we call Mode A and Mode B. Both of these modes attempt to
	//command the vehicle state in an absolute sense as much as possible (e.g. specifying height instead of vertical velocity)
	//but it is not possible to specify absolute 2D position in VirtualStickMode, so we specify 2D velocity in either the
	//vehicle body frame or in East-North. Because we are commanding velocity, we need to worry about what happens if our software
	//crashes - we certainly don't want the drones to keep moving with their last-commanded velocity. Thus, we include a timout
	//field for each virtual stick command. If another virtual stick command isn't received by the client App within the timeout
	//window, it should issue it's own new virtual stick command with the same values as the most recent received command except
	//with the 2D velocity fields set to 0. This way, commands also serve as a heartbeat signal from the GCS and if they stop coming,
	//the drones hover (without changing modes though).
	
	//To use a ModeA virtual stick command, the drone should be configured as follows:
	//DJIVirtualStickVerticalControlMode    = DJIVirtualStickVerticalControlModePosition
	//DJIVirtualStickRollPitchControlMode   = DJIVirtualStickRollPitchControlModeVelocity
	//DJIVirtualStickYawControlMode         = DJIVirtualStickYawControlModeAngle
	//DJIVirtualStickFlightCoordinateSystem = DJIVirtualStickFlightCoordinateSystemGround
	//In this mode, yaw is specified in an absolute sense, relative to North. Height is commanded in an absolute sense, relative to ground,
	//2D position is controlled by commanding vehicle velocity in the North and East directions
	struct VirtualStickCommand_ModeA {
		float Yaw     = 0.0f;   //Radians: 0 corresponds to North, positive is clockwise rotation
		float V_North = 0.0f;   //m/s: North component of vehicle velocity (Acceptable range -15 to 15)
		float V_East  = 0.0f;   //m/s: East component of vehicle velocity (Acceptable range -15 to 15)
		float HAG     = 10.0f;  //m: Height above ground (vehicle altitude - takeoff altitude)
		float timeout = 2.0f;   //s: If a new command isn't received within this time, the drone should hover
		
		VirtualStickCommand_ModeA() = default;
		~VirtualStickCommand_ModeA() = default;
		
		//If switching to C++20, default this
		bool operator==(VirtualStickCommand_ModeA const & Other) const {
			return (this->Yaw       == Other.Yaw)       &&
			       (this->V_North   == Other.V_North)   &&
			       (this->V_East    == Other.V_East)    &&
			       (this->HAG       == Other.HAG)       &&
			       (this->timeout   == Other.timeout);
		}
	};
	
	//To use a ModeB virtual stick command, the drone should be configured as follows:
	//DJIVirtualStickVerticalControlMode    = DJIVirtualStickVerticalControlModePosition
	//DJIVirtualStickRollPitchControlMode   = DJIVirtualStickRollPitchControlModeVelocity
	//DJIVirtualStickYawControlMode         = DJIVirtualStickYawControlModeAngle
	//DJIVirtualStickFlightCoordinateSystem = DJIVirtualStickFlightCoordinateSystemBody
	//In this mode, yaw is specified in an absolute sense, relative to North. Height is commanded in an absolute sense, relative to ground,
	//2D position is controlled by commanding vehicle velocity in vehicle body frame (forward and vehicle right).
	struct VirtualStickCommand_ModeB {
		float Yaw       = 0.0f;  //Radians: 0 corresponds to North, positive is clockwise rotation
		float V_Forward = 0.0f;  //m/s: Forward component of vehicle velocity (Acceptable range -15 to 15)
		float V_Right   = 0.0f;  //m/s: Vehicle-Right component of vehicle velocity (Acceptable range -15 to 15)
		float HAG       = 10.0f; //m: Height above ground (vehicle altitude - takeoff altitude)
		float timeout   = 2.0f;  //s: If a new command isn't received within this time, the drone should hover
		
		VirtualStickCommand_ModeB() = default;
		~VirtualStickCommand_ModeB() = default;
		
		//If switching to C++20, default this
		bool operator==(VirtualStickCommand_ModeB const & Other) const {
			return (this->Yaw       == Other.Yaw)       &&
			       (this->V_Forward == Other.V_Forward) &&
			       (this->V_Right   == Other.V_Right)   &&
			       (this->HAG       == Other.HAG)       &&
			       (this->timeout   == Other.timeout);
		}
	};
	
	//Waypoint objects are used as components of WaypointMission objects. Note that the speed field should be checked before putting it in a DJIWaypoint.
	//If it is 0, it needs to be adjusted upwards to a default min value. If 0 is put in a DJIWaypoint speed field, the behavior changes and the speed
	//gets overwritten by another value set at the mission level. We don't want this ridiculous behavior so we should make sure this is never actually 0.
	//Note that this data structure has changed to use relative instead of absolute altitude to address the fact that DJI drones have increadibly poor
	//knowledge of their absolute altitudes.
	struct Waypoint {
		double Latitude  = 0.0; //WGS84 Latitude of waypoint (Radians)
		double Longitude = 0.0; //WGS84 Longitude of waypoint (Radians)
		//double Altitude  = 0.0; //WGS84 Altitude of waypoint (meters) - Note that this is actual altitude and not height above ground
		double RelAltitude = 0.0; //Height above home point for waypoint (meters)
		
		float CornerRadius = 0.2f; //Radius of arc (m) to make when cutting corner at this waypoint. Only used when CurvedTrajectory = true in the parent mission.
		float Speed = 1.0f;        //Vehicle speed (m/s) between this waypoint and the next waypoint (0 < Speed <= 15)
		
		//Waypoint Actions: The drone can be told to execute certain actions once reaching a waypoint. Actions are not mutually exclusive (according to the
		//docs anyways) so none, one, or multiple can be used in a waypoint. Important Note: Actions are only executed if CurvedTrajectory = false in the parent mission.
		//For each action field, the value NaN indicates that the action should not be included. A non-NaN value generally indicates that an action should be added
		//to the waypoint to accomplish the given goal. These fields correspond to the following waypoint actions:
		//LoiterTime:  DJIWaypointActionTypeStay
		//GimbalPitch: DJIWaypointActionTypeRotateGimbalPitch
		float LoiterTime  = std::nanf("");  //Time (s) to hover at this waypoint (0 is equivilent to NaN and should result in the action not being included).
		float GimbalPitch = std::nanf("");  //Pitch of Gimbal, if connected (DJI Definition) in radians at waypoint.
		
		Waypoint() = default;
		~Waypoint() = default;
		
		//If switching to C++20, default this
		bool operator==(Waypoint const & Other) const {
			return (this->Latitude     == Other.Latitude)     &&
			       (this->Longitude    == Other.Longitude)    &&
			       (this->RelAltitude  == Other.RelAltitude)  &&
			       (this->CornerRadius == Other.CornerRadius) &&
			       (this->Speed        == Other.Speed)        &&
			       (this->LoiterTime   == Other.LoiterTime)   &&
			       (this->GimbalPitch  == Other.GimbalPitch);
		}
	};
	
	inline std::ostream & operator<<(std::ostream & Str, Waypoint const & v) { 
		double PI = 3.14159265358979;
		Str << "Latitude ----: " << 180.0/PI*v.Latitude     << " degrees\r\n";
		Str << "Longitude ---: " << 180.0/PI*v.Longitude    << " degrees\r\n";
		Str << "RelAltitude -: " <<          v.RelAltitude  << " m\r\n";
		Str << "CornerRadius : " <<          v.CornerRadius << " m\r\n";
		Str << "Speed -------: " <<          v.Speed        << " m/s\r\n";
		Str << "LoiterTime --: " <<          v.LoiterTime   << " s\r\n";
		Str << "GimbalPitch -: " << 180.0/PI*v.GimbalPitch  << " degrees\r\n";
		return Str;
	}

	//Get the distance (m) between two waypoints, in a 2D (East-North) sense.
	//DJI drones don't really know their true altitude (everything works relative to takeoff alt), so the most natural things to
	//do, which is to project the locations to the same local-level plane at the waypoints average altitude, and compute their distance,
	//isn't really possible. Instead we project both waypoints down to the reference ellipsoid and compute distance. Consequently, this
	//is an approximation, but generally a pretty good one unless the waypoints are very far apart or extremely far from sea level.
	inline double DistBetweenWaypoints2D(Waypoint const & WPA, Waypoint const & WPB) {
		Eigen::Vector3d WPA_ECEF = LLA2ECEF(Eigen::Vector3d(WPA.Latitude, WPA.Longitude, 0.0));
		Eigen::Vector3d WPB_ECEF = LLA2ECEF(Eigen::Vector3d(WPB.Latitude, WPB.Longitude, 0.0));
		return (WPB_ECEF - WPA_ECEF).norm();
	}

	//Get the distance (m) between two waypoints, in a 3D sense (actual distance).
	//As with the 2D distance function, we can't do this properly because DJI drones don't know their true absolute altitudes and work
	//with relative altitude. Consequently this function gives an approximation to 3D distance. It should generally be a pretty good one
	//though, unless the waypoints are very far apart or extremely far from sea level.
	inline double DistBetweenWaypoints3D(Waypoint const & WPA, Waypoint const & WPB) {
		Eigen::Vector3d WPA_ECEF = LLA2ECEF(Eigen::Vector3d(WPA.Latitude, WPA.Longitude, 0.0));
		Eigen::Vector3d WPB_ECEF = LLA2ECEF(Eigen::Vector3d(WPB.Latitude, WPB.Longitude, 0.0));
		double distance2D   = (WPB_ECEF - WPA_ECEF).norm();
		double distanceVert = std::fabs(WPB.RelAltitude - WPA.RelAltitude);
		return std::sqrt(distance2D*distance2D + distanceVert*distanceVert);
	}
	
	//This struct holds a waypoint mission for a single drone. The full DJI waypoint mission interface is relatively complex - we only implement the
	//subset of it's functionality that we expect to be useful for our purposes. Note that for all these missions, the vehicle Heading Mode should be set to
	//DJIWaypointMissionHeadingAuto, which orients the aircraft so the front is always pointed in the direction of motion.
	//DJIWaypointMissionGotoWaypointMode should be set to DJIWaypointMissionGotoWaypointPointToPoint. This means the vehicle goes directly from it's
	//current location to the first waypoint, rather than changing 2D position and altitude separately. If you want the vehicle to change position vertically,
	//add a waypoint above or below another.
	struct WaypointMission {
		std::vector<Waypoint> Waypoints; //Waypoints to fly to, in order from the vehicle starting position (which is not included as a waypoint)
		bool LandAtLastWaypoint = false; //If true, the vehicle lands after the final waypoint. If false, the vehicle hovers in P flight mode after last waypoint.
		bool CurvedTrajectory   = false; //If true, cut corners near waypoints, giving curved trajectory. If false, fly point-to-point, stopping at each waypoint.
		
		WaypointMission() = default;
		~WaypointMission() = default;
		
		bool empty() { return Waypoints.empty(); }
		
		//If switching to C++20, default this
		bool operator==(WaypointMission const & Other) const {
			if ((this->LandAtLastWaypoint != Other.LandAtLastWaypoint) || (this->CurvedTrajectory != Other.CurvedTrajectory))
				return false;
			if (this->Waypoints.size() != Other.Waypoints.size())
				return false;
			for (size_t n = 0U; n < Waypoints.size(); n++) {
				if (! (this->Waypoints[n] == Other.Waypoints[n]))
					return false;
			}
			return true;
		}

		//Get the total horizontal travel distance for a mission (in m). If StartPos is not null, includes travel from start pos to waypoint 0
		double TotalMissionDistance2D(Waypoint const * StartPos) const {
			if (Waypoints.empty())
				return 0.0;

			double totalDistance = 0.0;
			if (StartPos != nullptr)
				totalDistance += DistBetweenWaypoints2D(*StartPos, Waypoints[0]);
			for (size_t n = 0U; n + 1U < Waypoints.size(); n++)
				totalDistance += DistBetweenWaypoints2D(Waypoints[n], Waypoints[n + 1U]);
			return totalDistance;
		}

		//Get the total 3D travel distance for a mission (in m). If StartPos is not null, includes travel from start pos to waypoint 0
		double TotalMissionDistance3D(Waypoint const * StartPos) const {
			if (Waypoints.empty())
				return 0.0;

			double totalDistance = 0.0;
			if (StartPos != nullptr)
				totalDistance += DistBetweenWaypoints3D(*StartPos, Waypoints[0]);
			for (size_t n = 0U; n + 1U < Waypoints.size(); n++)
				totalDistance += DistBetweenWaypoints3D(Waypoints[n], Waypoints[n + 1U]);
			return totalDistance;
		}
	};

	inline std::ostream & operator<<(std::ostream & Str, WaypointMission const & M) { 
		Str << "*****   Waypoint Mission   *****\r\n";
		Str << "LandAtLastWaypoint: " << (M.LandAtLastWaypoint ? "True" : "False") << "\r\n";
		Str << "CurvedTrajectory: " << (M.CurvedTrajectory ? "True" : "False") << "\r\n";
		Str << "Waypoints:\r\n";
		for (auto const & WP : M.Waypoints)
			Str << WP << "\r\n";
		return Str;
	}
}

