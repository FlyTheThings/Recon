//DroneComms provides data structures and serialization/deserialization support for socket communications between server and client
//Author: Bryan Poling
//Copyright (c) 2021 Sentek Systems, LLC. All rights reserved. 

//External Includes
#include <opencv2/imgcodecs.hpp>

//Project Includes
#include "DroneComms.hpp"

#define PI 3.14159265358979

// ****************************************************************************************************************************************
// ***************************************************   Standard-type Field encoders   ***************************************************
// ****************************************************************************************************************************************
static void encodeField_uint8 (std::vector<uint8_t> & Buffer, uint8_t  x) { Buffer.push_back(x); }

static void encodeField_uint16 (std::vector<uint8_t> & Buffer, uint16_t x) {
	Buffer.push_back((uint8_t) (x >> 8 ));
	Buffer.push_back((uint8_t)  x       );
}

static void encodeField_uint32 (std::vector<uint8_t> & Buffer, uint32_t x) {
	Buffer.push_back((uint8_t) (x >> 24));
	Buffer.push_back((uint8_t) (x >> 16));
	Buffer.push_back((uint8_t) (x >> 8 ));
	Buffer.push_back((uint8_t)  x       );
}

static void encodeField_uint64 (std::vector<uint8_t> & Buffer, uint64_t x) {
	Buffer.push_back((uint8_t) (x >> 56));
	Buffer.push_back((uint8_t) (x >> 48));
	Buffer.push_back((uint8_t) (x >> 40));
	Buffer.push_back((uint8_t) (x >> 32));
	Buffer.push_back((uint8_t) (x >> 24));
	Buffer.push_back((uint8_t) (x >> 16));
	Buffer.push_back((uint8_t) (x >> 8 ));
	Buffer.push_back((uint8_t)  x       );
}

static void encodeField_int8   (std::vector<uint8_t> & Buffer, int8_t  x) {encodeField_uint8 (Buffer, (uint8_t)  x);}
static void encodeField_int16  (std::vector<uint8_t> & Buffer, int16_t x) {encodeField_uint16(Buffer, (uint16_t) x);}
static void encodeField_int32  (std::vector<uint8_t> & Buffer, int32_t x) {encodeField_uint32(Buffer, (uint32_t) x);}
static void encodeField_int64  (std::vector<uint8_t> & Buffer, int64_t x) {encodeField_uint64(Buffer, (uint64_t) x);}
static void encodeField_float32(std::vector<uint8_t> & Buffer, float  x)  {encodeField_uint32(Buffer, reinterpret_cast<uint32_t &>(x));}
static void encodeField_float64(std::vector<uint8_t> & Buffer, double x)  {encodeField_uint64(Buffer, reinterpret_cast<uint64_t &>(x));}

//Note: Technically, the memory encoding of floating-point values is separate from machine "Endianness" but it is usually the case
//that floating point "Endianness" matches integer endianness and this code will work. However, if our integers are decoding correctly and
//all of our floats are not then we might be dealing with an obnoxious mixed-endianness platform/Architecture and we will need to encode
//and decode floats differently on that platform.

// ****************************************************************************************************************************************
// ***************************************************   Compound-type Field encoders   ***************************************************
// ****************************************************************************************************************************************
static void encodeField_String (std::vector<uint8_t> & Buffer, const std::string & x) {
	encodeField_uint32(Buffer, (uint32_t) x.size());
	for (char item : x)
		Buffer.push_back((uint8_t) item);
}

static void encodeField_Image (std::vector<uint8_t> & Buffer, cv::Mat const & x) {
	if (x.type() != CV_8UC3) {
		std::cerr << "Internal Error in encodeField_Image(): Only 3-channel (RGB) 8-bit depth images supported.\r\n";
		return;
	}
	encodeField_uint16(Buffer, (uint16_t) x.rows);
	encodeField_uint16(Buffer, (uint16_t) x.cols);
	for (int row = 0; row < x.rows; row++) {
		for (int col = 0; col < x.cols; col++) {
			cv::Vec3b pixel = x.at<cv::Vec3b>(row, col);
			encodeField_uint8(Buffer, (uint8_t) pixel(2)); //Red
			encodeField_uint8(Buffer, (uint8_t) pixel(1)); //Green
			encodeField_uint8(Buffer, (uint8_t) pixel(0)); //Blue
		}
	}
}

static void encodeField_CompressedImage (std::vector<uint8_t> & Buffer, cv::Mat const & x) {
	if (x.type() != CV_8UC3) {
		std::cerr << "Internal Error in encodeField_CompressedImage(): Only 3-channel (RGB) 8-bit depth images supported.\r\n";
		return;
	}
	
	std::vector<uint8_t> buff;
	std::vector<int> param(2);
	param[0] = cv::IMWRITE_JPEG_QUALITY;
	param[1] = 95; //Quality parameter: default(95) 0-100
	cv::imencode(".jpg", x, buff, param);
	Buffer.insert(Buffer.end(), buff.begin(), buff.end());
}


// ****************************************************************************************************************************************
// ***************************************************   Standard-type Field decoders   ***************************************************
// ****************************************************************************************************************************************
static uint8_t decodeField_uint8 (std::vector<uint8_t>::const_iterator & Iter) { return(*Iter++); }

static uint16_t decodeField_uint16 (std::vector<uint8_t>::const_iterator & Iter) {
	uint16_t value;
	value  = (uint16_t) *Iter++;  value <<= 8;
	value += (uint16_t) *Iter++;
	return(value);
}

static uint32_t decodeField_uint32 (std::vector<uint8_t>::const_iterator & Iter) {
	uint32_t value;
     value  = (uint32_t) *Iter++;  value <<= 8;
     value += (uint32_t) *Iter++;  value <<= 8;
     value += (uint32_t) *Iter++;  value <<= 8;
     value += (uint32_t) *Iter++;
     return(value);
}

static uint64_t decodeField_uint64 (std::vector<uint8_t>::const_iterator & Iter) {
	uint64_t value;
     value  = (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;  value <<= 8;
     value += (uint64_t) *Iter++;
     return(value);
}

static int8_t   decodeField_int8   (std::vector<uint8_t>::const_iterator & Iter) {return((int8_t) *Iter++);}
static int16_t  decodeField_int16  (std::vector<uint8_t>::const_iterator & Iter) {return((int16_t) decodeField_uint16(Iter));}
static int32_t  decodeField_int32  (std::vector<uint8_t>::const_iterator & Iter) {return((int32_t) decodeField_uint32(Iter));}
static int64_t  decodeField_int64  (std::vector<uint8_t>::const_iterator & Iter) {return((int64_t) decodeField_uint64(Iter));}

static float decodeField_float32 (std::vector<uint8_t>::const_iterator & Iter) {
	uint32_t bitPattern = decodeField_uint32(Iter);
	return reinterpret_cast<float &>(bitPattern);
}

static double decodeField_float64 (std::vector<uint8_t>::const_iterator & Iter) {
	uint64_t bitPattern = decodeField_uint64(Iter);
	return reinterpret_cast<double &>(bitPattern);
}

//Note: See comment about floats in field encoder section above.

// ****************************************************************************************************************************************
// ***************************************************   Compound-type Field decoders   ***************************************************
// ****************************************************************************************************************************************
//MaxBytes is the maximum number of bytes that can belong to the full string object (size field included). This is a reference argument that will be
//decremented by the number of decoded bytes. If the string field advertises a length that would make the full string object exceed this number of
//bytes, then we replace the advertised length with the largest safe value.
static std::string decodeField_String (std::vector<uint8_t>::const_iterator & Iter, unsigned int & MaxBytes) {
	if (MaxBytes < 4U) {
		fprintf(stderr,"Warning in decodeField_String: Not enough bytes left for string. Aborting decode.\r\n");
		Iter += MaxBytes;
		MaxBytes = 0U;
		return std::string();
	}
	
	unsigned int size = (unsigned int) decodeField_uint32(Iter);
	unsigned int bytesForObject = size + 4U;
	if (bytesForObject > MaxBytes) {
		fprintf(stderr,"Warning in decodeField_String: Not enough bytes left for string. Aborting decode.\r\n");
		Iter += MaxBytes;
		MaxBytes = 0U;
		return std::string();
	}
	
	std::string S;
	for (unsigned int n = 0U; n < size; n++)
		S.push_back((char) *Iter++);
	MaxBytes -= bytesForObject;
	return(S);
}

//MaxBytes is the maximum number of bytes that can belong to the full Image object (row/col fields included). This is a reference argument that will be
//decremented by the number of decoded bytes. If the field advertises a size that would make the full Image object exceed this number of
//bytes, then we replace the advertised length with the largest safe value.
static cv::Mat decodeField_Image (std::vector<uint8_t>::const_iterator & Iter, unsigned int & MaxBytes) {
	if (MaxBytes < 4U) {
		fprintf(stderr,"Warning in decodeField_Image: Not enough bytes left for Image. Aborting decode.\r\n");
		Iter += MaxBytes;
		MaxBytes = 0U;
		return cv::Mat(0, 0, CV_8UC3);
	}
	
	uint16_t rows = decodeField_uint16(Iter);
	uint16_t cols = decodeField_uint16(Iter);
	uint32_t totalBytes = uint32_t(rows)*uint32_t(cols)*uint32_t(3U) + uint32_t(4U);
	if (totalBytes > MaxBytes) {
		fprintf(stderr,"Warning in decodeField_Image: Not enough bytes left for Image. Aborting decode.\r\n");
		Iter += MaxBytes;
		MaxBytes = 0U;
		return cv::Mat(0, 0, CV_8UC3);
	}
	
	cv::Mat Image(rows, cols, CV_8UC3);
	for (int row = 0; row < Image.rows; row++) {
		for (int col = 0; col < Image.cols; col++) {
			uint8_t R = decodeField_uint8(Iter);
			uint8_t G = decodeField_uint8(Iter);
			uint8_t B = decodeField_uint8(Iter);
			
			cv::Vec3b & pixel(Image.at<cv::Vec3b>(row, col));
			pixel(0) = B;
			pixel(1) = G;
			pixel(2) = R;
		}
	}
	return Image;
}

static cv::Mat decodeField_CompressedImage (std::vector<uint8_t>::const_iterator & Iter, unsigned int & NumBytes) {
	std::vector<uint8_t> buffer(Iter, Iter + NumBytes); //Copy from source to local buffer
	
	cv::Mat image = cv::imdecode(buffer, cv::IMREAD_COLOR);
	if (image.type() != CV_8UC3) {
		std::cerr << "Internal Error in decodeField_CompressedImage(): imdecode yielded something other than a 3-channel (RGB) 8-bit depth image.\r\n";
		return cv::Mat();
	}
	return image;
}


namespace DroneInterface {
	// ****************************************************************************************************************************************
	// ******************************************************   Packet Implementation   *******************************************************
	// ****************************************************************************************************************************************
	void Packet::Clear(void) {
		m_data.clear();
		M_highLevelFieldsValid = false;
	}
	
	bool Packet::IsFinished(void) {
		if (m_data.size() < 7U)
			return false;
		if (! M_highLevelFieldsValid) {
			auto iter = m_data.cbegin() + 2;
			m_size = decodeField_uint32(iter);
			m_PID  = decodeField_uint8(iter);
			M_highLevelFieldsValid = true;
		}
		return (m_data.size() >= (size_t) m_size);
	}

	bool Packet::BytesNeeded(uint32_t & ByteCount) {
		if (m_data.size() < 7U)
			return false;
		if (! M_highLevelFieldsValid) {
			auto iter = m_data.cbegin() + 2;
			m_size = decodeField_uint32(iter);
			m_PID  = decodeField_uint8(iter);
			M_highLevelFieldsValid = true;
		}
		ByteCount = m_size - uint32_t(m_data.size());
		return true;
	}
	
	//Search the buffer for the sync field - if we find it, throw out everything before it from m_data. If we don't end up finding it, we clear the buffer
	//This if used to attempt re-synchronization if we receive a corrupt packet or lose synchronization for some reason.
	//This function guarentees that m_data will be smaller on exit than it was on entry (unless it was empty to start with).
	void Packet::ForwardScanForSync(void) {
		if (m_data.empty())
			return;
		for (size_t newHeadIndex = 1U; newHeadIndex + 1U < m_data.size(); newHeadIndex++) {
			if ((m_data[newHeadIndex] == (uint8_t) 218) && (m_data[newHeadIndex + 1] == (uint8_t) 167)) {
				//We found the sync field
				std::cerr << "Forward scan successful.\r\n";
				std::cerr << "newHeadIndex: " << newHeadIndex << "\r\n";
				std::cerr << "m_data.size(): " << m_data.size() << "\r\n";
				std::cerr << "m_data[newHeadIndex]: " << (unsigned int) m_data[newHeadIndex] << "\r\n";
				std::cerr << "m_data[newHeadIndex + 1]: " << (unsigned int) m_data[newHeadIndex + 1] << "\r\n";
				m_data.erase(m_data.begin(), m_data.begin() + newHeadIndex);
				M_highLevelFieldsValid = false;
				return;
			}
		}
		if ((m_data.size() > 1U) && (m_data.back() == (uint8_t) 218)) {
			//The last byte might be the start of a sync field
			m_data = std::vector<uint8_t>(1, (uint8_t) 218);
			M_highLevelFieldsValid = false;
			return;
		}
		m_data.clear();
		M_highLevelFieldsValid = false;
	}

	//Take total packet size and PID and add sync, size, and PID fields to m_data
	void Packet::AddHeader(uint32_t Size, uint8_t PID) {
		encodeField_uint16(m_data, (uint16_t) 55975U);
		encodeField_uint32(m_data, Size);
		encodeField_uint8 (m_data, PID);
	}

	//Based on current contents of m_data (which should be fully populated except for the hash field) compute and add hash field
	void Packet::AddHash(void) {
		uint8_t hashA = 0U;
		uint8_t hashB = 0U;
		for (size_t n = 0U; n < m_data.size(); n++) {
			hashA += m_data[n];
			hashB += hashA;
		}
		encodeField_uint8(m_data, hashA);
		encodeField_uint8(m_data, hashB);
	}

	//Returns false if not enough data to decode PID
	bool Packet::GetPID(uint8_t & PID) const {
		if (m_data.size() < 7U)
			return(false); //Not enough data in buffer yet
		auto iter = m_data.cbegin() + 6U;
		PID = decodeField_uint8(iter);
		return true;
	}

	//Returns true if m_data has correct size and passes hash check and false otherwise
	bool Packet::CheckHash(void) const {
		if (m_data.size() < 9U)
			return false; //Packet cannot be valid because it is below min size
		auto iter = m_data.cbegin() + 2U;
		uint32_t size = decodeField_uint32(iter);
		if (size_t(size) != m_data.size())
			return false; //Packet cannot be valid because it's size is different than advertised
		uint8_t hashA = 0U;
		uint8_t hashB = 0U;
		for (size_t n = 0U; n < m_data.size() - 2U; n++) {
			hashA += m_data[n];
			hashB += hashA;
		}
		return ((hashA == m_data[m_data.size() - 2U]) && (hashB == m_data[m_data.size() - 1U]));
	}

	//Returns true if PID matches, size matches advertised size, and hash is good.
	bool Packet::CheckHashSizeAndPID(uint8_t PID) const {
		if (! CheckHash())
			return false;
		auto iter = m_data.cbegin() + 6U;
		return (decodeField_uint8(iter) == PID);
	}


	// ****************************************************************************************************************************************
	// ************************************************   Packet_CoreTelemetry Implementation   ***********************************************
	// ****************************************************************************************************************************************
	bool Packet_CoreTelemetry::operator==(Packet_CoreTelemetry const & Other) const {
		return (this->IsFlying  == Other.IsFlying)  &&
		       (this->Latitude  == Other.Latitude)  &&
		       (this->Longitude == Other.Longitude) &&
		       (this->Altitude  == Other.Altitude)  &&
		       (this->HAG       == Other.HAG)       &&
		       (this->V_N       == Other.V_N)       &&
		       (this->V_E       == Other.V_E)       &&
		       (this->V_D       == Other.V_D)       &&
		       (this->Yaw       == Other.Yaw)       &&
		       (this->Pitch     == Other.Pitch)     &&
		       (this->Roll      == Other.Roll);
	}
	
	void Packet_CoreTelemetry::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 69U), uint8_t(0U));
		encodeField_uint8  (TargetPacket.m_data, IsFlying);
		encodeField_float64(TargetPacket.m_data, Latitude);
		encodeField_float64(TargetPacket.m_data, Longitude);
		encodeField_float64(TargetPacket.m_data, Altitude);
		encodeField_float64(TargetPacket.m_data, HAG);
		encodeField_float32(TargetPacket.m_data, V_N);
		encodeField_float32(TargetPacket.m_data, V_E);
		encodeField_float32(TargetPacket.m_data, V_D);
		encodeField_float64(TargetPacket.m_data, Yaw);
		encodeField_float64(TargetPacket.m_data, Pitch);
		encodeField_float64(TargetPacket.m_data, Roll);
		TargetPacket.AddHash();
	}

	bool Packet_CoreTelemetry::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 0U))
			return false;
		if (SourcePacket.m_data.size() != 9U + 69U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		IsFlying  = decodeField_uint8(iter);
		Latitude  = decodeField_float64(iter);
		Longitude = decodeField_float64(iter);
		Altitude  = decodeField_float64(iter);
		HAG       = decodeField_float64(iter);
		V_N       = decodeField_float32(iter);
		V_E       = decodeField_float32(iter);
		V_D       = decodeField_float32(iter);
		Yaw       = decodeField_float64(iter);
		Pitch     = decodeField_float64(iter);
		Roll      = decodeField_float64(iter);
		return true;
	}

	// ****************************************************************************************************************************************
	// **********************************************   Packet_ExtendedTelemetry Implementation   *********************************************
	// ****************************************************************************************************************************************
	bool Packet_ExtendedTelemetry::operator==(Packet_ExtendedTelemetry const & Other) const {
		return (this->GNSSSatCount == Other.GNSSSatCount) &&
		       (this->GNSSSignal   == Other.GNSSSignal)   &&
		       (this->MaxHeight    == Other.MaxHeight)    &&
		       (this->MaxDist      == Other.MaxDist)      &&
		       (this->BatLevel     == Other.BatLevel)     &&
		       (this->BatWarning   == Other.BatWarning)   &&
		       (this->WindLevel    == Other.WindLevel)    &&
		       (this->DJICam       == Other.DJICam)       &&
		       (this->FlightMode   == Other.FlightMode)   &&
		       (this->MissionID    == Other.MissionID)    &&
		       (this->DroneSerial  == Other.DroneSerial);
	}
	
	void Packet_ExtendedTelemetry::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 12U + 4U + DroneSerial.size()), uint8_t(1U));
		encodeField_uint16(TargetPacket.m_data, GNSSSatCount);
		encodeField_uint8 (TargetPacket.m_data, GNSSSignal);
		encodeField_uint8 (TargetPacket.m_data, MaxHeight);
		encodeField_uint8 (TargetPacket.m_data, MaxDist);
		encodeField_uint8 (TargetPacket.m_data, BatLevel);
		encodeField_uint8 (TargetPacket.m_data, BatWarning);
		encodeField_uint8 (TargetPacket.m_data, WindLevel);
		encodeField_uint8 (TargetPacket.m_data, DJICam);
		encodeField_uint8 (TargetPacket.m_data, FlightMode);
		encodeField_uint16(TargetPacket.m_data, MissionID);
		encodeField_String(TargetPacket.m_data, DroneSerial);
		TargetPacket.AddHash();
	}

	bool Packet_ExtendedTelemetry::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 1U))
			return false;
		if (SourcePacket.m_data.size() < 9U + 16U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		GNSSSatCount = decodeField_uint16(iter);
		GNSSSignal   = decodeField_uint8(iter);
		MaxHeight    = decodeField_uint8(iter);
		MaxDist      = decodeField_uint8(iter);
		BatLevel     = decodeField_uint8(iter);
		BatWarning   = decodeField_uint8(iter);
		WindLevel    = decodeField_uint8(iter);
		DJICam       = decodeField_uint8(iter);
		FlightMode   = decodeField_uint8(iter);
		MissionID    = decodeField_uint16(iter);
		
		unsigned int MaxBytesForStr = (unsigned int) SourcePacket.m_data.size() - 9U - 12U;
		DroneSerial = decodeField_String(iter, MaxBytesForStr);
		return true;
	}


	// ****************************************************************************************************************************************
	// ****************************************************   Packet_Image Implementation   ***************************************************
	// ****************************************************************************************************************************************
	bool Packet_Image::operator==(Packet_Image const & Other) const {
		if (this->TargetFPS != Other.TargetFPS)
			return false;
		if ((this->Frame.rows != Other.Frame.rows) || (this->Frame.cols != Other.Frame.cols))
			return false;
		if (this->Frame.type() != Other.Frame.type())
			return false;
		for (int row = 0; row < this->Frame.rows; row++) {
			for (int col = 0; col < this->Frame.cols; col++) {
				cv::Vec3b A = this->Frame.at<cv::Vec3b>(row, col);
				cv::Vec3b B = Other.Frame.at<cv::Vec3b>(row, col);
				if ((A(0) != B(0)) || (A(1) != B(1)) || (A(2) != B(2)))
					return false;
			}
		}
		return true;
	}
	
	void Packet_Image::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 4U + 4U + (unsigned int)(Frame.rows*Frame.cols*3)), uint8_t(2U));
		encodeField_float32(TargetPacket.m_data, TargetFPS);
		encodeField_Image  (TargetPacket.m_data, Frame);
		TargetPacket.AddHash();
	}

	bool Packet_Image::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 2U))
			return false;
		if (SourcePacket.m_data.size() < 9U + 8U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		TargetFPS = decodeField_float32(iter);
		
		unsigned int MaxBytesForImage = (unsigned int) SourcePacket.m_data.size() - 9U - 4U;
		Frame = decodeField_Image(iter, MaxBytesForImage);
		return true;
	}
	
	
	// ****************************************************************************************************************************************
	// ***********************************************   Packet_CompressedImage Implementation   **********************************************
	// ****************************************************************************************************************************************
	bool Packet_CompressedImage::operator==(Packet_CompressedImage const & Other) const {
		if (this->TargetFPS != Other.TargetFPS)
			return false;
		if ((this->Frame.rows != Other.Frame.rows) || (this->Frame.cols != Other.Frame.cols))
			return false;
		if (this->Frame.type() != Other.Frame.type())
			return false;
		for (int row = 0; row < this->Frame.rows; row++) {
			for (int col = 0; col < this->Frame.cols; col++) {
				cv::Vec3b A = this->Frame.at<cv::Vec3b>(row, col);
				cv::Vec3b B = Other.Frame.at<cv::Vec3b>(row, col);
				if ((A(0) != B(0)) || (A(1) != B(1)) || (A(2) != B(2)))
					return false;
			}
		}
		return true;
	}
	
	void Packet_CompressedImage::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(0U), uint8_t(5U)); //Use a dummy value (0) for size since we only know it after compression
		encodeField_float32(TargetPacket.m_data, TargetFPS);
		encodeField_CompressedImage(TargetPacket.m_data, Frame);
		
		//Update the header with the correct packet size
		uint32_t packetSize = (uint32_t) TargetPacket.m_data.size() + 2U; //Don't forget the hash field, which hasn't been added yet
		TargetPacket.m_data[2] = (uint8_t) (packetSize >> 24);
		TargetPacket.m_data[3] = (uint8_t) (packetSize >> 16);
		TargetPacket.m_data[4] = (uint8_t) (packetSize >> 8 );
		TargetPacket.m_data[5] = (uint8_t) (packetSize      );
		
		//Add hash after updating the packet size field
		TargetPacket.AddHash();
	}
	
	bool Packet_CompressedImage::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 5U))
			return false;
		if (SourcePacket.m_data.size() < 9U + 4U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		TargetFPS = decodeField_float32(iter);
		
		unsigned int JPEGNumBytes = (unsigned int) SourcePacket.m_data.size() - 9U - 4U;
		Frame = decodeField_CompressedImage(iter, JPEGNumBytes);
		return true;
	}


	// ****************************************************************************************************************************************
	// ************************************************   Packet_Acknowledgment Implementation   **********************************************
	// ****************************************************************************************************************************************
	bool Packet_Acknowledgment::operator==(Packet_Acknowledgment const & Other) const {
		return (this->Positive  == Other.Positive) &&
		       (this->SourcePID == Other.SourcePID);
	}
	
	void Packet_Acknowledgment::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 2U), uint8_t(3U));
		encodeField_uint8(TargetPacket.m_data, Positive);
		encodeField_uint8(TargetPacket.m_data, SourcePID);
		TargetPacket.AddHash();
	}

	bool Packet_Acknowledgment::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 3U))
			return false;
		if (SourcePacket.m_data.size() != 9U + 2U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		Positive  = decodeField_uint8(iter);
		SourcePID = decodeField_uint8(iter);
		return true;
	}


	// ****************************************************************************************************************************************
	// ************************************************   Packet_MessageString Implementation   ***********************************************
	// ****************************************************************************************************************************************
	bool Packet_MessageString::operator==(Packet_MessageString const & Other) const {
		return (this->Type    == Other.Type) &&
		       (this->Message == Other.Message);
	}
	
	void Packet_MessageString::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 1U + 4U + Message.size()), uint8_t(4U));
		encodeField_uint8 (TargetPacket.m_data, Type);
		encodeField_String(TargetPacket.m_data, Message);
		TargetPacket.AddHash();
	}

	bool Packet_MessageString::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 4U))
			return false;
		if (SourcePacket.m_data.size() < 9U + 5U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		Type = decodeField_uint8(iter);
		
		unsigned int MaxBytesForStr = (unsigned int) SourcePacket.m_data.size() - 9U - 1U;
		Message = decodeField_String(iter, MaxBytesForStr);
		return true;
	}


	// ****************************************************************************************************************************************
	// **********************************************   Packet_EmergencyCommand Implementation   **********************************************
	// ****************************************************************************************************************************************
	bool Packet_EmergencyCommand::operator==(Packet_EmergencyCommand const & Other) const {
		return (this->Action == Other.Action);
	}
	
	void Packet_EmergencyCommand::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 1U), uint8_t(255U));
		encodeField_uint8(TargetPacket.m_data, Action);
		TargetPacket.AddHash();
	}

	bool Packet_EmergencyCommand::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 255U))
			return false;
		if (SourcePacket.m_data.size() != 9U + 1U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		Action = decodeField_uint8(iter);
		return true;
	}
	
	
	// ****************************************************************************************************************************************
	// ************************************************   Packet_CameraControl Implementation   ***********************************************
	// ****************************************************************************************************************************************
	bool Packet_CameraControl::operator==(Packet_CameraControl const & Other) const {
		return (this->Action    == Other.Action) &&
		       (this->TargetFPS == Other.TargetFPS);
	}
	
	void Packet_CameraControl::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 5U), uint8_t(254U));
		encodeField_uint8  (TargetPacket.m_data, Action);
		encodeField_float32(TargetPacket.m_data, TargetFPS);
		TargetPacket.AddHash();
	}
	
	bool Packet_CameraControl::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 254U))
			return false;
		if (SourcePacket.m_data.size() != 9U + 5U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		Action    = decodeField_uint8(iter);
		TargetFPS = decodeField_float32(iter);
		return true;
	}
	
	
	// ****************************************************************************************************************************************
	// ********************************************   Packet_ExecuteWaypointMission Implementation   ******************************************
	// ****************************************************************************************************************************************
	bool Packet_ExecuteWaypointMission::operator==(Packet_ExecuteWaypointMission const & Other) const {
		if ((this->LandAtEnd != Other.LandAtEnd) || (this->CurvedFlight != Other.CurvedFlight))
			return false;
		if (this->Waypoints.size() != Other.Waypoints.size())
			return false;
		for (size_t n = 0U; n < this->Waypoints.size(); n++) {
			if (! (this->Waypoints[n] == Other.Waypoints[n]))
				return false;
		}
		return true;
	}
	
	void Packet_ExecuteWaypointMission::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 2U + 40U*((unsigned int) Waypoints.size())), uint8_t(253U));
		encodeField_uint8(TargetPacket.m_data, LandAtEnd);
		encodeField_uint8(TargetPacket.m_data, CurvedFlight);
		for (auto const & waypoint : Waypoints) {
			encodeField_float64(TargetPacket.m_data, waypoint.Latitude*180.0/PI);
			encodeField_float64(TargetPacket.m_data, waypoint.Longitude*180.0/PI);
			encodeField_float64(TargetPacket.m_data, waypoint.RelAltitude);
			encodeField_float32(TargetPacket.m_data, waypoint.CornerRadius);
			encodeField_float32(TargetPacket.m_data, waypoint.Speed);
			encodeField_float32(TargetPacket.m_data, waypoint.LoiterTime);
			encodeField_float32(TargetPacket.m_data, waypoint.GimbalPitch*180.0/PI);
		}
		TargetPacket.AddHash();
	}
	
	bool Packet_ExecuteWaypointMission::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 253U))
			return false;
		if (SourcePacket.m_data.size() < 9U + 2U + 40U)
			return false;
		
		auto iter    = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		LandAtEnd    = decodeField_uint8(iter);
		CurvedFlight = decodeField_uint8(iter);
		
		Waypoints.clear();
		unsigned int waypointBytes = (unsigned int) SourcePacket.m_data.size() - 9U - 2U;
		if (waypointBytes % 40U != 0U) {
			std::cerr << "Error in Packet_ExecuteWaypointMission::Deserialize(): Unacceptable payload size.\r\n";
			return false;
		}
		unsigned int numWaypoints = waypointBytes / 40U;
		Waypoints.reserve(numWaypoints);
		for (unsigned int n = 0U; n < numWaypoints; n++) {
			Waypoints.emplace_back();
			Waypoints.back().Latitude     = decodeField_float64(iter)*PI/180.0;
			Waypoints.back().Longitude    = decodeField_float64(iter)*PI/180.0;
			Waypoints.back().RelAltitude  = decodeField_float64(iter);
			Waypoints.back().CornerRadius = decodeField_float32(iter);
			Waypoints.back().Speed        = decodeField_float32(iter);
			Waypoints.back().LoiterTime   = decodeField_float32(iter);
			Waypoints.back().GimbalPitch  = decodeField_float32(iter)*PI/180.0;
		}
		return true;
	}
	
	
	// ****************************************************************************************************************************************
	// *********************************************   Packet_VirtualStickCommand Implementation   ********************************************
	// ****************************************************************************************************************************************
	bool Packet_VirtualStickCommand::operator==(Packet_VirtualStickCommand const & Other) const {
		return (this->Mode    == Other.Mode) &&
		       (this->Yaw     == Other.Yaw)  &&
		       (this->V_x     == Other.V_x)  &&
		       (this->V_y     == Other.V_y)  &&
		       (this->HAG     == Other.HAG)  &&
		       (this->timeout == Other.timeout);
	}
	
	void Packet_VirtualStickCommand::Serialize(Packet & TargetPacket) const {
		TargetPacket.Clear();
		TargetPacket.AddHeader(uint32_t(9U + 21U), uint8_t(252U));
		encodeField_uint8  (TargetPacket.m_data, Mode);
		
		//Map to degrees in the range [-180, 180]
		float yaw_deg = Yaw*180.0/PI;
		yaw_deg = fmod(yaw_deg, 360.0);
		if (yaw_deg < 0.0)
			yaw_deg += 360.0;
		if (yaw_deg > 180.0)
			yaw_deg -= 360.0;
		
		encodeField_float32(TargetPacket.m_data, yaw_deg);
		encodeField_float32(TargetPacket.m_data, V_x);
		encodeField_float32(TargetPacket.m_data, V_y);
		encodeField_float32(TargetPacket.m_data, HAG);
		encodeField_float32(TargetPacket.m_data, timeout);
		TargetPacket.AddHash();
	}
	
	bool Packet_VirtualStickCommand::Deserialize(Packet const & SourcePacket) {
		if (! SourcePacket.CheckHashSizeAndPID((uint8_t) 252U))
			return false;
		if (SourcePacket.m_data.size() != 9U + 21U)
			return false;
		
		auto iter = SourcePacket.m_data.cbegin() + 7U; //Const iterater to begining of payload
		Mode    = decodeField_uint8(iter);
		Yaw     = decodeField_float32(iter) * PI/180.0;
		V_x     = decodeField_float32(iter);
		V_y     = decodeField_float32(iter);
		HAG     = decodeField_float32(iter);
		timeout = decodeField_float32(iter);
		return true;
	}
	
	
	// ****************************************************************************************************************************************
	// ***************************************************   Stream Operator Definitions   ****************************************************
	// ****************************************************************************************************************************************
	std::ostream & operator<<(std::ostream & Str, Packet_CoreTelemetry const & v) { 
		Str << "IsFlying -: " << (unsigned int) v.IsFlying  << "\r\n";
		Str << "Latitude -: " << v.Latitude  << " degrees\r\n";
		Str << "Longitude : " << v.Longitude << " degrees\r\n";
		Str << "Altitude -: " << v.Altitude  << " m\r\n";
		Str << "HAG ------: " << v.HAG       << " m\r\n";
		Str << "V_N ------: " << v.V_N       << " m/s\r\n";
		Str << "V_E ------: " << v.V_E       << " m/s\r\n";
		Str << "V_D ------: " << v.V_D       << " m/s\r\n";
		Str << "Yaw ------: " << v.Yaw       << " degrees\r\n";
		Str << "Pitch ----: " << v.Pitch     << " degrees\r\n";
		Str << "Roll -----: " << v.Roll      << " degrees\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_ExtendedTelemetry const & v) { 
		Str << "GNSSSatCount : " << (unsigned int) v.GNSSSatCount << "\r\n";
		Str << "GNSSSignal --: " << (unsigned int) v.GNSSSignal   << "\r\n";
		Str << "MaxHeight ---: " << (unsigned int) v.MaxHeight    << "\r\n";
		Str << "MaxDist -----: " << (unsigned int) v.MaxDist      << "\r\n";
		Str << "BatLevel ----: " << (unsigned int) v.BatLevel     << "\r\n";
		Str << "BatWarning --: " << (unsigned int) v.BatWarning   << "\r\n";
		Str << "WindLevel ---: " << (unsigned int) v.WindLevel    << "\r\n";
		Str << "DJICam ------: " << (unsigned int) v.DJICam       << "\r\n";
		Str << "FlightMode --: " << (unsigned int) v.FlightMode   << "\r\n";
		Str << "MissionID ---: " << (unsigned int) v.MissionID    << "\r\n";
		Str << "DroneSerial -: " <<                v.DroneSerial  << "\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_Image const & v) { 
		Str << "TargetFPS : " << v.TargetFPS << " frame/s\r\n";
		Str << "Frame ----: " << v.Frame.rows << " x " << v.Frame.cols << " Image\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_Acknowledgment const & v) { 
		if (v.Positive > uint8_t(0))
			Str << "Positive acknowledgement of: ";
		else
			Str << "Negative acknowledgement of: ";
		switch (v.SourcePID) {
			case uint8_t(255): Str << "Emergency Command";        break;
			case uint8_t(254): Str << "Camera Control";           break;
			case uint8_t(253): Str << "Execute Waypoint Mission"; break;
			case uint8_t(252): Str << "Virtual Stick Command";    break;
			default:           Str << "Unrecognized (PID = " << (unsigned int) v.SourcePID << ")";
		}
		Str << " packet\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_MessageString const & v) { 
		switch (v.Type) {
			case uint8_t(0): Str << "Debug";   break;
			case uint8_t(1): Str << "Info";    break;
			case uint8_t(2): Str << "Warning"; break;
			case uint8_t(3): Str << "Error";   break;
			default:         Str << "Unrecognized (Type = " << (unsigned int) v.Type << ")";
		}
		Str << " message received: " << v.Message << "\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_EmergencyCommand const & v) { 
		Str << "Action : " << (unsigned int) v.Action << "\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_CameraControl const & v) { 
		Str << "Action ---: " << (unsigned int) v.Action    << "\r\n";
		Str << "TargetFPS : " <<                v.TargetFPS << " frame/s\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_ExecuteWaypointMission const & v) { 
		Str << "LandAtEnd ---: " << (unsigned int) v.LandAtEnd        << "\r\n";
		Str << "CurvedFlight : " << (unsigned int) v.CurvedFlight     << "\r\n";
		Str << "Waypoints ---: " << (unsigned int) v.Waypoints.size() << " items\r\n";
		for (auto const & waypoint : v.Waypoints)
			Str << waypoint << "\r\n";
		return Str;
	}
	
	std::ostream & operator<<(std::ostream & Str, Packet_VirtualStickCommand const & v) { 
		Str << "Mode ---: " << (unsigned int) v.Mode    << "\r\n";
		Str << "Yaw ----: " <<                v.Yaw     << " degrees\r\n";
		Str << "V_x ----: " <<                v.V_x     << " m/s\r\n";
		Str << "V_y ----: " <<                v.V_y     << " m/s\r\n";
		Str << "HAG ----: " <<                v.HAG     << " m\r\n";
		Str << "timeout : " <<                v.timeout << " s\r\n";
		return Str;
	}
}




