//This module provides utility functions for coordinate conversions, tile lookup, and related things
//Author: Bryan Poling
//Copyright (c) 2020 Sentek Systems, LLC. All rights reserved. 
#pragma once

//System Includes
#include <cstdint>
#include <tuple>
#include <cmath>

//Eigen Includes
#include "../../../eigen/Eigen/Core"

//We have two map coordinate systems. The first is Widget coordinates. This is the position of a pixel relative to the upper-left corner of the
//map widget, in pixels. X goes right and y goes down. The second coordinate system is Normalized Mercator, which is used to reference
//locations on the Earth. We have utilities to map back and forth between NM and widget coordinates. We also sometimes need to go back and
//forth to Lat/Lon, but normalized mercator should be used internally whenever possible for consistency.

inline Eigen::Vector2d NMToLatLon(Eigen::Vector2d const & NMCoords) {
	const double PI = 3.14159265358979;
	double x = NMCoords(0);
	double y = NMCoords(1);
	double lon = PI*x;
	double lat = 2.0*(atan(exp(y*PI)) - PI/4.0);
	return Eigen::Vector2d(lat,lon);
}

inline Eigen::Vector2d LatLonToNM(Eigen::Vector2d const & LatLon) {
	const double PI = 3.14159265358979;
	double lat = LatLon(0);
	double lon = LatLon(1);
	double x = lon / PI;
	double y = log(tan(lat/2.0 + PI/4.0)) / PI;
	return Eigen::Vector2d(x,y);
}

//Compute ECEF position (in meters) from WGS84 latitude, longitude, and altitude.
//Input vector is: [Lat (radians), Lon (radians), Alt (m)]
inline Eigen::Vector3d LLA2ECEF(Eigen::Vector3d const & Position_LLA) {
	double lat = Position_LLA(0);
	double lon = Position_LLA(1);
	double alt = Position_LLA(2);
	
	double a = 6378137.0;           //Semi-major axis of reference ellipsoid
	double ecc = 0.081819190842621; //First eccentricity of the reference ellipsoid
	double eccSquared = ecc*ecc;
	double N = a/sqrt(1.0 - eccSquared*sin(lat)*sin(lat));
	double X = (N + alt)*cos(lat)*cos(lon);
	double Y = (N + alt)*cos(lat)*sin(lon);
	double Z = (N*(1 - eccSquared) + alt)*sin(lat);
	
	return Eigen::Vector3d(X, Y, Z);
}

//Compute latitude (radians), longitude (radians), and altitude (height above WGS84 ref. elipsoid, in meters) from ECEF position (in meters)
//Latitude and longitude are also both given with respect to the WGS84 reference elipsoid.
//Output is the vector [Lat (radians), Lon (radians), Alt (m)]
inline Eigen::Vector3d ECEF2LLA(Eigen::Vector3d const & Position_ECEF) {
	double x = Position_ECEF(0);
	double y = Position_ECEF(1);
	double z = Position_ECEF(2);
	
	//Set constants
	double R_0 = 6378137.0;
	double R_P = 6356752.314;
	double ecc = 0.081819190842621;
	
	//Calculate longitude (radians)
	double lon = atan2(y, x);
	
	//Compute intermediate values needed for lat and alt
	double eccSquared = ecc*ecc;
	double p = sqrt(x*x + y*y);
	double E = sqrt(R_0*R_0 - R_P*R_P);
	double F = 54.0*(R_P*z)*(R_P*z);
	double G = p*p + (1.0 - eccSquared)*z*z - eccSquared*E*E;
	double c = pow(ecc,4.0)*F*p*p/pow(G,3.0);
	double s = pow(1.0 + c + sqrt(c*c + 2.0*c),1.0/3.0);
	double P = (F/(3.0*G*G))/((s + (1.0/s) + 1.0)*(s + (1.0/s) + 1.0));
	double Q = sqrt(1.0 + 2.0*pow(ecc,4.0)*P);
	double k_1 = -1.0*P*eccSquared*p/(1.0 + Q);
	double k_2 = 0.5*R_0*R_0*(1.0 + 1.0/Q);
	double k_3 = -1.0*P*(1.0 - eccSquared)*z*z/(Q*(1.0 + Q));
	double k_4 = -0.5*P*p*p;
	double r_0 = k_1 + sqrt(k_2 + k_3 + k_4);
	double k_5 = (p - eccSquared*r_0);
	double U = sqrt(k_5*k_5 + z*z);
	double V = sqrt(k_5*k_5 + (1.0 - eccSquared)*z*z);

	double z_0 = (R_P*R_P*z)/(R_0*V);
	double e_p = (R_0/R_P)*ecc;
	
	//Calculate latitude (radians)
	double lat = atan((z + z_0*e_p*e_p)/p);

	//Calculate Altitude (m)
	double alt = U*(1.0 - (R_P*R_P/(R_0*V)));
	
	return Eigen::Vector3d(lat, lon, alt);
}

//Get the rotation between ECEF and ENU at a given latitude and longitude
inline Eigen::Matrix3d latLon_2_C_ECEF_ENU(double lat, double lon) {
	//Populate C_ECEF_NED
	Eigen::Matrix3d C_ECEF_NED;
	C_ECEF_NED << -sin(lat)*cos(lon), -sin(lat)*sin(lon),  cos(lat),
	              -sin(lon),                    cos(lon),       0.0,
	              -cos(lat)*cos(lon), -cos(lat)*sin(lon), -sin(lat);
	
	//Compute C_ECEF_ENU from C_ECEF_NED
	Eigen::Matrix3d C_NED_ENU;
	C_NED_ENU << 0.0,  1.0,  0.0,
	             1.0,  0.0,  0.0,
	             0.0,  0.0, -1.0;
	Eigen::Matrix3d C_ECEF_ENU = C_NED_ENU * C_ECEF_NED;
	return C_ECEF_ENU;
}

inline Eigen::Vector2d WidgetCoordsToNormalizedMercator(Eigen::Vector2d const & WidgetCords, Eigen::Vector2d const & ULCorner_NM, double Zoom, int32_t tileWidth) {
	double screenPixelLengthNM = 2.0 / (pow(2.0, Zoom) * ((double) tileWidth));
	return (ULCorner_NM + Eigen::Vector2d(WidgetCords(0)*screenPixelLengthNM, -1.0*WidgetCords(1)*screenPixelLengthNM));
}

inline Eigen::Vector2d NormalizedMercatorToWidgetCoords(Eigen::Vector2d const & NMCoords, Eigen::Vector2d const & ULCorner_NM, double Zoom, int32_t tileWidth) {
	double screenPixelLengthNM = 2.0 / (pow(2.0, Zoom) * ((double) tileWidth));
	return Eigen::Vector2d(NMCoords(0) - ULCorner_NM(0), ULCorner_NM(1) - NMCoords(1))/screenPixelLengthNM;
}

//Convert a distance in meters to normalized mercator units at the given point on Earth (only the y-coord matters).
//Be careful with this - it is not perfect and gets especially bad over long distances.
inline double MetersToNMUnits(double Meters, double yPos_NM) {
	const double PI = 3.14159265358979;
	const double C = 40075017.0; //meters (Wikipedia)
	
	double lat = 2.0*(atan(exp(yPos_NM*PI)) - PI/4.0);
	double NMUnitsPerMeter = 2.0/(C*cos(lat));
	return Meters*NMUnitsPerMeter;
}

//Convert a distance in normalized mercator units to meters at a given point on Earth (only the y-coord matters).
//This is not exact and should certainly never be used over large distances.
inline double NMUnitsToMeters(double Dist_NM, double yPos_NM) {
	const double PI = 3.14159265358979;
	const double C = 40075017.0; //meters (Wikipedia)
	
	double lat = 2.0*(atan(exp(yPos_NM*PI)) - PI/4.0);
	double MetersPerNMUnit = C*cos(lat)/2.0;
	return Dist_NM*MetersPerNMUnit;
}

//Convert a distance in meters to pixels at the given zoom level and point on Earth (only the y-coord matters).
//Be careful with this - it is not perfect and gets especially bad over long distances.
inline double MetersToPixels(double Meters, double yPos_NM, double MapZoom) {
	const double PI = 3.14159265358979;
	const double C = 40075017.0; //meters (Wikipedia)
	
	double lat = 2.0*(atan(exp(yPos_NM*PI)) - PI/4.0);
	double PixelsPerMeter = pow(2.0, MapZoom + 8.0)/(C*cos(lat));
	return Meters*PixelsPerMeter;
}

//Convert a distance in pixels at a given zoom level to NM units at the given point on Earth (only the y-coord matters).
//Be careful with this - it is not perfect and gets especially bad over long distances.
inline double PixelsToNMUnits(double Pixels, double yPos_NM, double MapZoom) {
	const double PI = 3.14159265358979;
	const double C = 40075017.0; //meters (Wikipedia)
	
	double lat = 2.0*(atan(exp(yPos_NM*PI)) - PI/4.0);
	double NMUnitsPerMeter = 2.0/(C*cos(lat));
	double PixelsPerMeter = pow(2.0, MapZoom + 8.0)/(C*cos(lat));
	double NMUnitsPerPixel = NMUnitsPerMeter / PixelsPerMeter;
	return Pixels*NMUnitsPerPixel;
}

//Take the Upper-Left corner location of a map in Normalized Mercator coordinates and the zoom level and map dimensions (Width x Height in piexls) and get
//the X and Y limits of the viewable area in Normalized Mercator coordinates. Returned in form (XMin, XMax, YMin, YMax)
inline Eigen::Vector4d GetViewableArea_NormalizedMercator(Eigen::Vector2d const & ULCorner_NM, Eigen::Vector2d const & WindowDims, double Zoom, int32_t tileWidth) {
	Eigen::Vector2d LRCorner_NM = WidgetCoordsToNormalizedMercator(WindowDims, ULCorner_NM, Zoom, tileWidth);
	return Eigen::Vector4d(ULCorner_NM(0), LRCorner_NM(0), LRCorner_NM(1), ULCorner_NM(1));
}

//Get the Normalized-Mercator coordinates of the center of pixel (row, col) in the given tile
inline Eigen::Vector2d TilePixelToNM(int32_t TileX, int32_t TileY, int32_t PyramidLevel, int Row, int Col, int32_t tileWidth) {
	int32_t tilesOnThisLevel = (int32_t) (1U << PyramidLevel); //In each dimension
	double xNM = (double(TileX) + (double(Col) + 0.5)/double(tileWidth))*2.0 / (double(tilesOnThisLevel)) - 1.0;
	double yNM = 1.0 - (double(TileY) + (double(Row) + 0.5)/double(tileWidth))*2.0 / double(tilesOnThisLevel);
	return Eigen::Vector2d(xNM, yNM);
}

//Get the pixel coordinates of the given Normalized-Mercator position in the given tile. Returned in form <col, row>.
//This is the inverse of TilePixelToNM()
inline Eigen::Vector2d NMToTilePixel(int32_t TileX, int32_t TileY, int32_t PyramidLevel, Eigen::Vector2d const & Position_NM, int32_t tileWidth) {
	int32_t tilesOnThisLevel = (int32_t) (1U << PyramidLevel); //In each dimension
	double Col = ((1.0 + Position_NM(0))*double(tilesOnThisLevel)/2.0 - double(TileX))*double(tileWidth) - 0.5;
	double Row = ((1.0 - Position_NM(1))*double(tilesOnThisLevel)/2.0 - double(TileY))*double(tileWidth) - 0.5;
	return Eigen::Vector2d(Col, Row);
}

//Get the pixel containing the given Normalized-Mercator position in the given tile. Returned in form <col, row>. This version
//saturates each coordinate to [0, tileWidth-1]
inline std::tuple<int, int> NMToTilePixel_int(int32_t TileX, int32_t TileY, int32_t PyramidLevel, Eigen::Vector2d const & Position_NM, int32_t tileWidth) {
	int32_t tilesOnThisLevel = (int32_t) (1U << PyramidLevel); //In each dimension
	double Col = ((1.0 + Position_NM(0))*double(tilesOnThisLevel)/2.0 - double(TileX))*double(tileWidth) - 0.5;
	double Row = ((1.0 - Position_NM(1))*double(tilesOnThisLevel)/2.0 - double(TileY))*double(tileWidth) - 0.5;
	Col = floor(std::min(std::max(Col, 0.0), double(tileWidth - 1)));
	Row = floor(std::min(std::max(Row, 0.0), double(tileWidth - 1)));
	return std::make_tuple(int(Col), int(Row));
}

//Get <tileCol, tileRow> for the tile on the given pyramid level containing the given point in Normalized Mercator coordinates
inline std::tuple<int32_t, int32_t> getCoordsOfTileContainingPoint(Eigen::Vector2d const & PointNM, int32_t PyramidLevel) {
	int32_t tilesOnThisLevel = (int32_t) (1U << PyramidLevel); //In each dimension
	int32_t tileX = floor((PointNM(0) + 1.0)*tilesOnThisLevel/2.0);
	int32_t tileY = floor((1.0 - PointNM(1))*tilesOnThisLevel/2.0);
	tileX = std::max((int32_t) 0, std::min(tilesOnThisLevel - 1, tileX));
	tileY = std::max((int32_t) 0, std::min(tilesOnThisLevel - 1, tileY));
	return std::make_tuple(tileX, tileY);
}

//Get NM coords of upper-left corner of upper-left pixel of a given tile
inline Eigen::Vector2d GetNMCoordsOfULCornerOfTile(int32_t TileX, int32_t TileY, int32_t PyramidLevel) {
	int32_t tilesOnThisLevel = (int32_t) (1U << PyramidLevel); //In each dimension
	double xNM = ((double) TileX)*2.0 / ((double) tilesOnThisLevel) - 1.0;
	double yNM = 1.0 - ((double) TileY)*2.0 / ((double) tilesOnThisLevel);
	return Eigen::Vector2d(xNM, yNM);
}

//Get NM coords of lower-right corner of lower-right pixel of a given tile
inline Eigen::Vector2d GetNMCoordsOfLRCornerOfTile(int32_t TileX, int32_t TileY, int32_t PyramidLevel) {
	int32_t tilesOnThisLevel = (int32_t) (1U << PyramidLevel); //In each dimension
	double xNM = ((double) (TileX+1))*2.0 / ((double) tilesOnThisLevel) - 1.0;
	double yNM = 1.0 - ((double) (TileY+1))*2.0 / ((double) tilesOnThisLevel);
	return Eigen::Vector2d(xNM, yNM);
}




